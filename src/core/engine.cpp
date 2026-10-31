#include "modsynth_iram.h"
#include "engine.h"
#include <cmath>
#include <new>
#include "delay_pool.h"
#include "kit_groups.h"
#include "dsp_util.h"
#include "modules/dly.h"
#include "modules/keys.h"
#include "modules/kit.h"

namespace {
// A module created while bypassed: its out buffers (zero, or input 0 on output 0) and params,
// without the DSP or its heap. It becomes the real module when enabled.
class BypassStub final : public Module {
   public:
    explicit BypassStub(const ModuleTypeInfo& t) : Module(t) {}
    void process(const float* const*, const bool*) override {}
};
}  // namespace

// Bypassed and without an instance: a stub, except KEYS (tiny, and its PLAY page needs the real
// one). Stubs need some heap too, so only above half the reserve.
Module* Engine::makeStub(const ModuleTypeInfo& t) const {
    if (heapFree_() <= kRamReserve / 2) return nullptr;
    return new (std::nothrow) BypassStub(t);
}

bool Engine::stubbed(const ModuleRec& r) { return r.bypass && r.type != &kKeysType; }

static_assert(kit::kLanes == 8, "Engine::lanes_");

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

namespace {
int poolUsed(const Patch& p) {
    int used = 0;
    for (int i = 0; i < p.moduleCount(); i++) used += p.moduleAt(i).type->poolSlots;
    return used;
}
}  // namespace

uint32_t Engine::ramFor(const Patch& p, const ModuleTypeInfo& t) {
    return t.ramBytes + (t.poolSlots ? delayPoolHeapBytes(poolUsed(p), t.poolSlots) : 0);
}

Refusal Engine::fits(const Patch& p, const ModuleTypeInfo& t, bool cpu) const {
    if (t.poolSlots && poolUsed(p) + t.poolSlots > kDelaySlots) return {false, "NO BUFFER"};
    if (t.maxInstances && p.countOfType(t) >= t.maxInstances) return {false, "MAX 1"};
    if (!cpu) return {true, nullptr};
    if (costOf(t) + patchCycles(p) > kCpuBudget) return {false, "NO CPU"};
    return {true, nullptr};
}

Refusal Engine::canAdd(const Patch& p, const ModuleTypeInfo& t) const {
    const Refusal r = fits(p, t);
    if (!r.ok) return r;
    if (heapFree_() < ramFor(p, t) + kRamReserve) return {false, "NO RAM"};
    return {true, nullptr};
}

Refusal Engine::canLoad(const Patch& sofar, const ModuleTypeInfo& t) const { return fits(sofar, t, false); }

bool Engine::canCreate(const ModuleTypeInfo& t) const {
    uint32_t need = t.ramBytes, big = t.largestAlloc ? t.largestAlloc : t.ramBytes;
    if (t.poolSlots) {
        for (const auto& r : retired_)  // its slot comes back at the next collect()
            if (r.m && r.m->type().poolSlots) return false;
        if (delayPoolStaticFree() == 0) {
            need += kDelaySlotBytes;
            big = big > kDelaySlotBytes ? big : kDelaySlotBytes;
        }
    }
    if (heapFree_() < need + kRamReserve) return false;
    return !largestBlock_ || largestBlock_() >= big + 1024;
}

Engine::Live Engine::live() const {
    return {(uint32_t)(cpuLoad() * (float)kCyclesPerSample), largestBlock_ ? largestBlock_() : UINT32_MAX};
}

Refusal Engine::canAddLive(const Patch& p, const ModuleTypeInfo& t) const { return canAddLive(p, t, live()); }

Refusal Engine::canAddLive(const Patch& p, const ModuleTypeInfo& t, const Live& l) const {
    // CPU from the measured load alone: it holds the running patch, cables and mixing included.
    // The per-type sum canAdd uses is each type's worst moment and over-counts a live patch.
    const Refusal r = fits(p, t, false);
    if (!r.ok) return r;
    // An idle KIT measures nothing: its worst step comes on top (over-counts while a step sounds).
    if (l.cycles + costOf(t) + kitWorst(p) > kCpuBudget) return {false, "NO CPU"};
    if (heapFree_() < ramFor(p, t) + kRamReserve) return {false, "NO RAM"};
    // Free heap can be plenty while no single block fits (GRD's 16 KB buffer, a 96 KB heap
    // delay slot).
    uint32_t big = t.largestAlloc ? t.largestAlloc : t.ramBytes;
    if (t.poolSlots && delayPoolHeapBytes(poolUsed(p), t.poolSlots) > big) big = kDelaySlotBytes;
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

uint32_t Engine::estimatedCycles() const { return patchCycles(patch_); }

namespace {
// Per-lane cost of p's grouped modules and the always-on rest; bypassed ones are not run.
int splitCosts(const Engine& e, const Patch& p, uint32_t* always, uint32_t* lane) {
    uint8_t g[kMaxModules];
    const int kit = kitGroups(p, g);
    *always = 0;
    for (int l = 0; l < kit::kLanes; l++) lane[l] = 0;
    for (int i = 0; i < p.moduleCount(); i++) {
        if (p.moduleAt(i).bypass) continue;
        const uint32_t c = e.costOf(*p.moduleAt(i).type);
        if (g[i] == kNoGroup) *always += c;
        else lane[g[i]] += c;
    }
    return kit;
}
}  // namespace

uint32_t Engine::kitWorst(const Patch& p) const {
    uint32_t always, lane[kit::kLanes];
    const int kit = splitCosts(*this, p, &always, lane);
    return kit < 0 ? 0 : kitWorstStep(p.moduleAt(kit), lane);
}

uint32_t Engine::patchCycles(const Patch& p) const {
    uint32_t always, lane[kit::kLanes];
    const int kit = splitCosts(*this, p, &always, lane);
    return always + (kit < 0 ? 0 : kitWorstStep(p.moduleAt(kit), lane));
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

Module* Engine::instance(uint16_t id) {
    const int k = indexOf(id);
    return k >= 0 && !insts_[k].stub ? insts_[k].m : nullptr;
}

int Engine::indexOf(uint16_t id) const {
    for (size_t k = 0; k < insts_.size(); k++)
        if (insts_[k].id == id) return (int)k;
    return -1;
}

void Engine::retire(Module* m, Program* p, uint32_t gen) { retired_.push_back({gen, m, p}); }

void Engine::pushValues(Module* m, const ModuleRec& r) {
    for (int k = 0; k < r.type->numParams && k < kMaxParams; k++) m->setParam(k, r.params[k]);
    for (int o = 0; o < r.type->numOuts && o < kMaxOuts; o++) m->setOutAmount(o, r.outAmount[o]);
    for (int i = 0; i < r.type->numIns && i < kMaxIns; i++) m->setInAmount(i, r.inAmount[i]);
}

int Engine::instantiateAll(const Patch& p, uint32_t gen) {
    int missing = 0;
    for (int i = 0; i < p.moduleCount(); i++) {
        const ModuleRec& r = p.moduleAt(i);
        const int k = indexOf(r.id);
        Module* m = k >= 0 ? insts_[k].m : nullptr;
        if (stubbed(r)) {  // an existing instance stays; a new one is only a stub (no heap)
            if (!m) {
                m = makeStub(*r.type);
                if (!m) {
                    missing++;
                    continue;
                }
                insts_.push_back({r.id, m, true});
            }
            pushValues(m, r);
            continue;
        }
        if (!m || insts_[k].stub) {
            Module* real = tryCreate(*r.type);
            if (!real) {
                missing++;
                continue;
            }
            if (m) {  // enabled: the stub goes once the audio thread has left the old program
                retire(m, nullptr, gen);
                insts_[k] = {r.id, real, false};
            } else {
                insts_.push_back({r.id, real, false});
            }
            m = real;
        }
        pushValues(m, r);
    }
    return missing;
}

void Engine::publish(uint32_t gen) {
    Module* inst[kMaxModules];
    for (int i = 0; i < patch_.moduleCount(); i++) {
        const ModuleRec& r = patch_.moduleAt(i);
        const int k = indexOf(r.id);
        // an enabled module still on its stub (no RAM yet) is missing, not silent-but-present
        inst[i] = k < 0 || (insts_[k].stub && !r.bypass) ? nullptr : insts_[k].m;
    }
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
    missing_ = instantiateAll(p, gen);
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
    const uint32_t gen = gen_ + 1;  // the program published below
    for (int i = 0; i < patch_.moduleCount(); i++) {
        const ModuleRec& r = patch_.moduleAt(i);
        const int k = indexOf(r.id);
        if (k >= 0 && (!insts_[k].stub || stubbed(r))) continue;
        Module* m = stubbed(r) ? makeStub(*r.type) : tryCreate(*r.type);  // as instantiateAll
        if (!m) {
            missing++;
            continue;
        }
        if (k >= 0) {
            retire(insts_[k].m, nullptr, gen);
            insts_[k] = {r.id, m, false};
        } else {
            insts_.push_back({r.id, m, stubbed(r)});
        }
        pushValues(m, r);
        any = true;
    }
    missing_ = missing;
    if (any) publish(++gen_);
}

// Audio thread. A new program starts with every group idle: their outputs read 0.
void Engine::startProgram(const Program* p) {
    for (int l = 0; l < kit::kLanes; l++) laneOff(p, l);
}

constexpr uint8_t kFadeBlocks = 4;  // 2.7 ms: a step's end ramps its group out, no click

void Engine::laneOff(const Program* p, int lane) {
    lanes_[lane] = {false, 0, 0, 0.f};
    for (int n = 0; n < p->numNodes; n++) {
        const ProgramNode& node = p->nodes[n];
        if (node.group != lane) continue;
        float* o = node.m->out(0);
        for (int k = 0; k < node.m->type().numOuts * kBlock; k++) o[k] = 0.f;
        node.m->setPeak(0.f);
        node.m->silence();
    }
}

// After the KIT's process(): a step edge fades out the groups the new step does not fire (one
// step's groups at a time, as the CPU guard counts them, give or take the fade); the fired ones
// start.
void Engine::kitEvents(uint32_t ev) {
    for (int l = 0; l < kit::kLanes; l++) {
        if ((ev >> l) & 1u) lanes_[l] = {true, 0, 0, 0.f};
        else if ((ev & kit::kStepEdge) && lanes_[l].on && !lanes_[l].fade) lanes_[l].fade = kFadeBlocks;
    }
}

MODSYNTH_IRAM void Engine::processBlock(float* mono) {
    for (int i = 0; i < kBlock; i++) mono[i] = 0.f;
    if (Program* p = pending_.exchange(nullptr, std::memory_order_acq_rel)) {
        cur_ = p;
        seen_.store(p->generation, std::memory_order_release);
        startProgram(p);
    }
    const Program* prog = cur_;
    if (!prog) return;
    const float* ins[kMaxIns];
    bool conn[kMaxIns];
    for (int n = 0; n < prog->numNodes; n++) {
        const ProgramNode& node = prog->nodes[n];
        Module* m = node.m;
        if (node.group != kNoGroup && !lanes_[node.group].on) continue;  // idle: outputs stay 0
        const uint32_t t0 = cycles_ ? cycles_() : 0;
        const int numIns = m->type().numIns;
        for (int in = 0; in < kMaxIns; in++) {
            conn[in] = in < numIns && node.nSrc[in] > 0;
            ins[in] = scratch_[in];
            if (!conn[in]) continue;
            if (node.nSrc[in] == 1) {  // one source at unity: read its buffer in place (not its own)
                const ProgramSource& src = prog->srcs[node.firstSrc[in]];
                if (src.m != m && src.m->outAmount(src.out) == 1.f && m->inAmount(in) == 1.f) {
                    ins[in] = src.m->out(src.out);
                    continue;
                }
            }
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
        uint32_t release;
        if (const uint32_t push = m->pushedInputs(&release); push | release) {  // v1.5 manual gates
            for (int in = 0; in < numIns; in++) {
                if (!((push | release) >> in & 1u)) continue;
                float* acc = scratch_[in];
                if (!conn[in]) {
                    for (int k = 0; k < kBlock; k++) acc[k] = 0.f;
                } else if (ins[in] != acc) {
                    for (int k = 0; k < kBlock; k++) acc[k] = ins[in][k];
                }
                conn[in] = true;
                ins[in] = acc;
                if (push >> in & 1u)
                    for (int k = 0; k < kBlock; k++) acc[k] += kGateHighVolts;
            }
        }
        const int numOuts = m->type().numOuts;
        if (node.bypass == Bypass::None) {
            m->process(ins, conn);
        } else {  // not processed: output 0 carries input 0 (Pass) or 0, the other outputs 0
            float* o = m->out(0);
            for (int k = 0; k < numOuts * kBlock; k++) o[k] = 0.f;
            if (node.bypass == Bypass::Pass && conn[0] && numOuts > 0)
                for (int k = 0; k < kBlock; k++) o[k] = ins[0][k];
        }
        if (numOuts > 0) {  // the peak is out 0's
            float* o = m->out(0);
            float pk = sanitizeBlock(o, kBlock);
            m->setPeak(pk);
            if (numOuts > 1) {
                const float rest = sanitizeBlock(o + kBlock, (numOuts - 1) * kBlock);
                pk = rest > pk ? rest : pk;
            }
            if (node.boundary) {
                Lane& ln = lanes_[node.group];
                if (pk > ln.peak) ln.peak = pk;
                if (ln.fade) {  // what leaves the group ramps from fade/4 to (fade-1)/4
                    const float g0 = (float)ln.fade / kFadeBlocks, dg = -1.f / (kFadeBlocks * kBlock);
                    for (int ch = 0; ch < numOuts; ch++)
                        for (int k = 0; k < kBlock; k++) o[ch * kBlock + k] *= g0 + dg * (float)(k + 1);
                }
            }
        }
        if (node.bypass != Bypass::None) continue;  // no master out, and its measured cost stays
        if (n == prog->kitNode) kitEvents(static_cast<KitModule*>(m)->events());
        if (const float* mo = m->masterOut())
            for (int k = 0; k < kBlock; k++) mono[k] += mo[k];
        if (cycles_) m->addCycles(cycles_() - t0);
    }
    // A group whose outputs leaving it stayed under -60 dB (of 5 V) for 50 ms is done.
    for (int l = 0; l < kit::kLanes; l++) {
        Lane& ln = lanes_[l];
        if (!ln.on) continue;
        ln.quiet = ln.peak < 0.005f ? ln.quiet + kBlock : 0;
        ln.peak = 0.f;
        if ((ln.fade && --ln.fade == 0) || ln.quiet >= (uint32_t)(0.05f * kSampleRate)) laneOff(prog, l);
    }
    sanitizeBlock(mono, kBlock);
    for (int k = 0; k < kBlock; k++) mono[k] = mono[k] > 1.f ? 1.f : (mono[k] < -1.f ? -1.f : mono[k]);
}
