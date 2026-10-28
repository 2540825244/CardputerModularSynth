#include "engine.h"
#include <cmath>
#include "delay_pool.h"
#include "dsp_util.h"
#include "modules/dly.h"

Engine::Engine(HeapFreeFn heapFree) : heapFree_(heapFree), scratch_{} {}

Engine::~Engine() {
    pending_.store(nullptr);
    delete published_;  // the newest program; older ones are on the retire list
    for (auto& r : retired_) {
        delete r.p;
        delete r.m;
    }
    for (auto& i : insts_) delete i.m;
}

Refusal Engine::canAdd(const Patch& p, const ModuleTypeInfo& t) const {
    const int have = p.countOfType(t);
    if (t.poolSlots) {
        int used = 0;
        for (int i = 0; i < p.moduleCount(); i++) used += p.moduleAt(i).type->poolSlots;
        if (used + t.poolSlots > kDelaySlots) return {false, "NO BUFFER"};
    }
    if (t.maxInstances && have >= t.maxInstances) return {false, "MAX 1"};
    uint32_t cycles = costOf(t);
    for (int i = 0; i < p.moduleCount(); i++) cycles += costOf(*p.moduleAt(i).type);
    if (cycles > kCpuBudget) return {false, "NO CPU"};
    if (heapFree_() < t.ramBytes + kRamReserve) return {false, "NO RAM"};
    return {true, nullptr};
}

Engine::Live Engine::live() const {
    return {(uint32_t)(cpuLoad() * (float)kCyclesPerSample), largestBlock_ ? largestBlock_() : UINT32_MAX};
}

Refusal Engine::canAddLive(const Patch& p, const ModuleTypeInfo& t) const { return canAddLive(p, t, live()); }

Refusal Engine::canAddLive(const Patch& p, const ModuleTypeInfo& t, const Live& l) const {
    const Refusal r = canAdd(p, t);
    if (!r.ok) return r;
    // The costs miss the engine's own work (cables, mixing) and anything not measured yet.
    if (l.cycles + costOf(t) > kCpuBudget) return {false, "NO CPU"};
    // Free heap can be plenty while no single block fits (GRD wants ~30 KB in one piece).
    const uint32_t big = t.largestAlloc ? t.largestAlloc : t.ramBytes;
    if (l.largest < big + 1024) return {false, "NO RAM"};
    return {true, nullptr};
}

uint32_t Engine::measuredCost(const ModuleTypeInfo& t) const {
    for (const auto& l : learned_)
        if (l.t == &t) return l.cycles;
    return 0;
}

uint32_t Engine::costOf(const ModuleTypeInfo& t) const {
    const uint32_t m = measuredCost(t);
    if (!m) return t.cpuCycles;
    const uint32_t padded = m + m / 8;  // headroom for cache misses the average hides
    return t.costVaries && t.cpuCycles > padded ? t.cpuCycles : padded;
}

uint32_t Engine::estimatedCycles() const {
    uint32_t c = 0;
    for (int i = 0; i < patch_.moduleCount(); i++) c += costOf(*patch_.moduleAt(i).type);
    return c;
}

void Engine::learnCosts() {
    for (const auto& in : insts_) {
        const uint32_t c = in.m->measuredCycles();
        if (!c) continue;
        Learned* l = nullptr;
        for (auto& x : learned_)
            if (x.t == &in.m->type()) l = &x;
        if (!l) {
            learned_.push_back({&in.m->type(), 0});
            l = &learned_.back();
        }
        // Only a rise of > 3% counts as news (the average wobbles).
        if (c > l->cycles + l->cycles / 32) costsChanged_ = true;
        if (c > l->cycles) l->cycles = c;
    }
}

bool Engine::costsChanged() {
    const bool r = costsChanged_;
    costsChanged_ = false;
    return r;
}

Module* Engine::instanceOf(uint16_t id) const {
    for (const auto& i : insts_)
        if (i.id == id) return i.m;
    return nullptr;
}

Module* Engine::instance(uint16_t id) { return instanceOf(id); }

void Engine::retire(Module* m, Program* p, uint32_t gen) { retired_.push_back({gen, m, p}); }

void Engine::pushValues(Module* m, const ModuleRec& r) {
    for (int k = 0; k < r.type->numParams && k < kMaxParams; k++) m->setParam(k, r.params[k]);
    for (int o = 0; o < r.type->numOuts && o < kMaxOuts; o++) m->setOutAmount(o, r.outAmount[o]);
    for (int i = 0; i < r.type->numIns && i < kMaxIns; i++) m->setInAmount(i, r.inAmount[i]);
}

int Engine::instantiateAll(const Patch& p) {
    int missing = 0;
    for (int i = 0; i < p.moduleCount(); i++) {
        const ModuleRec& r = p.moduleAt(i);
        Module* m = instanceOf(r.id);
        if (!m) {
            m = r.type->create();
            if (!m) {
                missing++;
                continue;
            }
            insts_.push_back({r.id, m});
        }
        pushValues(m, r);
    }
    return missing;
}

void Engine::publish(uint32_t gen) {
    Module* inst[kMaxModules];
    for (int i = 0; i < patch_.moduleCount(); i++) inst[i] = instanceOf(patch_.moduleAt(i).id);
    Program* prog = compileProgram(patch_, inst, gen);
    pending_.exchange(prog, std::memory_order_release);  // an unconsumed older program is still published_
    if (published_) retire(nullptr, published_, gen);
    published_ = prog;
}

int Engine::apply(const Patch& p) {
    const uint32_t gen = ++gen_;
    patch_ = p;
    // drop instances whose module is gone or changed type; they live until the audio thread has
    // moved on to a program that no longer references them
    for (size_t k = 0; k < insts_.size();) {
        const ModuleRec* r = p.find(insts_[k].id);
        if (!r || r->type != &insts_[k].m->type()) {
            retire(insts_[k].m, nullptr, gen);
            insts_.erase(insts_.begin() + k);
        } else {
            k++;
        }
    }
    missing_ = instantiateAll(p);
    publish(gen);
    return missing_;
}

void Engine::setParam(uint16_t id, uint8_t i, float v) {
    if (Module* m = instanceOf(id)) m->setParam(i, v);
    if (ModuleRec* r = patch_.find(id)) {
        if (i < kMaxParams) r->params[i] = v;
    }
}

void Engine::setOutAmount(uint16_t id, uint8_t o, float a) {
    if (Module* m = instanceOf(id)) m->setOutAmount(o, a);
    if (ModuleRec* r = patch_.find(id)) {
        if (o < kMaxOuts) r->outAmount[o] = a;
    }
}

void Engine::setInAmount(uint16_t id, uint8_t i, float a) {
    if (Module* m = instanceOf(id)) m->setInAmount(i, a);
    if (ModuleRec* r = patch_.find(id)) {
        if (i < kMaxIns) r->inAmount[i] = a;
    }
}

void Engine::collect() {
    const uint32_t seen = seen_.load(std::memory_order_acquire);
    size_t w = 0;
    bool freed = false;
    for (size_t k = 0; k < retired_.size(); k++) {
        if (retired_[k].gen <= seen) {
            freed = freed || retired_[k].m;
            delete retired_[k].p;
            delete retired_[k].m;
        } else {
            retired_[w++] = retired_[k];
        }
    }
    retired_.resize(w);
    if (cycles_ && (++collects_ & 63) == 0) learnCosts();
    // Retry modules that were refused: at once when a module was just freed (its pool slot or
    // heap is back), else every 64th call (~130 ms) - a failing create() churns big heap blocks.
    if (missing_ == 0 || (!freed && ++retryTick_ < 64)) return;
    retryTick_ = 0;
    bool any = false;
    int missing = 0;
    for (int i = 0; i < patch_.moduleCount(); i++) {
        const ModuleRec& r = patch_.moduleAt(i);
        if (instanceOf(r.id)) continue;
        Module* m = r.type->create();
        if (!m) {
            missing++;
            continue;
        }
        insts_.push_back({r.id, m});
        pushValues(m, r);
        any = true;
    }
    missing_ = missing;
    if (any) publish(++gen_);
}

void Engine::processBlock(float* mono) {
    for (int i = 0; i < kBlock; i++) mono[i] = 0.f;
    if (Program* p = pending_.exchange(nullptr, std::memory_order_acq_rel)) {
        cur_ = p;
        seen_.store(p->generation, std::memory_order_release);
    }
    const Program* prog = cur_;
    if (!prog) return;
    const float* ins[kMaxIns];
    bool conn[kMaxIns];
    for (int n = 0; n < prog->numNodes; n++) {
        const ProgramNode& node = prog->nodes[n];
        Module* m = node.m;
        const uint32_t t0 = cycles_ ? cycles_() : 0;
        const int numIns = m->type().numIns;
        for (int in = 0; in < kMaxIns; in++) {
            conn[in] = in < numIns && node.nSrc[in] > 0;
            ins[in] = scratch_[in];
            if (!conn[in]) continue;
            float* acc = scratch_[in];
            for (int k = 0; k < kBlock; k++) acc[k] = 0.f;
            for (int s = 0; s < node.nSrc[in]; s++) {
                const ProgramSource& src = prog->srcs[node.firstSrc[in] + s];
                const float* o = src.m->out(src.out);
                const float amt = src.m->outAmount(src.out);
                for (int k = 0; k < kBlock; k++) acc[k] += o[k] * amt;
            }
            const float ia = m->inAmount(in);
            if (ia != 1.f)
                for (int k = 0; k < kBlock; k++) acc[k] *= ia;
        }
        m->process(ins, conn);
        const int numOuts = m->type().numOuts;
        if (numOuts > 0) {
            float* o = m->out(0);
            float pk = 0.f;
            for (int k = 0; k < numOuts * kBlock; k++) o[k] = sanitize(o[k]);
            for (int k = 0; k < kBlock; k++) pk = fabsf(o[k]) > pk ? fabsf(o[k]) : pk;
            m->setPeak(pk);
        }
        if (const float* mo = m->masterOut())
            for (int k = 0; k < kBlock; k++) mono[k] += mo[k];
        if (cycles_) m->addCycles(cycles_() - t0);
    }
    for (int k = 0; k < kBlock; k++) {
        const float v = sanitize(mono[k]);
        mono[k] = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
    }
}
