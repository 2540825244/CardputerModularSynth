#include <unity.h>
#include <cmath>
#include "core/delay_pool.h"
#include "core/engine.h"
#include "core/registry.h"
#include <cstring>
#include "core/modules/dly.h"
#include "core/modules/env.h"
#include "core/modules/grd.h"
#include "core/modules/keys.h"
#include "core/modules/kit.h"
#include "core/modules/lfo.h"
#include "core/modules/mix.h"
#include "core/modules/src.h"
#include "core/kit_groups.h"
#include "core/modules/mod.h"
#include "core/program.h"
#include "core/modules/osc.h"
#include "core/modules/out.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"

void setUp() {}
void tearDown() {}

// ---- test-only module types ----
static int gDtorCount = 0;
static float gSinkIn = 0.f;
static bool gSinkConnected = false;
static bool gNanNow = false;

static const char* const kTIn[] = {"in"};
static const char* const kTOut[] = {"out"};

class DcSrc : public Module {  // 5 V constant
   public:
    DcSrc();
    void process(const float* const*, const bool*) override {
        for (int i = 0; i < kBlock; i++) out(0)[i] = 5.f;
    }
};
class Sink : public Module {  // records its input, outputs nothing useful
   public:
    Sink();
    void process(const float* const* ins, const bool* c) override {
        gSinkConnected = c[0];
        gSinkIn = c[0] ? ins[0][0] : 0.f;
        for (int i = 0; i < kBlock; i++) out(0)[i] = 0.f;
    }
};
class NanSrc : public Module {  // NaN while gNanNow, else a +-5 V square at Nyquist/2
   public:
    NanSrc();
    void process(const float* const*, const bool*) override {
        for (int i = 0; i < kBlock; i++)
            out(0)[i] = gNanNow ? NAN : (((i / 4) & 1) ? 5.f : -5.f);
    }
};
class Counted : public Module {  // passthrough with a destructor counter
   public:
    Counted();
    ~Counted() override { gDtorCount++; }
    void process(const float* const* ins, const bool* c) override {
        for (int i = 0; i < kBlock; i++) out(0)[i] = c[0] ? ins[0][i] : 0.f;
    }
};

extern const ModuleTypeInfo kDcType, kSinkType, kNanType, kCountedType;
#define TEST_TYPE(name, id, nIns, cls, maxInst)                                                  \
    static Module* create##cls() { return new cls(); }                                           \
    const ModuleTypeInfo name = {id, "TST", "test", Category::Source, Icon::Osc, Drawing::None, \
                                 0, nullptr, nIns, kTIn, kTIn, 1, kTOut, kTOut, 10, 100,         \
                                 maxInst, create##cls};
static Module* createDcSrc();
static Module* createSink();
static Module* createNanSrc();
static Module* createCounted();
TEST_TYPE(kDcType, "tdc", 0, DcSrc, 0)
TEST_TYPE(kSinkType, "tsink", 1, Sink, 0)
TEST_TYPE(kNanType, "tnan", 0, NanSrc, 0)
TEST_TYPE(kCountedType, "tcnt", 1, Counted, 0)
DcSrc::DcSrc() : Module(kDcType) {}
Sink::Sink() : Module(kSinkType) {}
NanSrc::NanSrc() : Module(kNanType) {}
Counted::Counted() : Module(kCountedType) {}

static uint32_t bigHeap() { return 1000000; }
static uint32_t smallHeap() { return 10 * 1024; }
static uint32_t gHeap = 0;
static uint32_t setHeap() { return gHeap; }

static uint16_t add(Patch& p, const ModuleTypeInfo& t, int cell) {
    uint16_t id = 0;
    TEST_ASSERT_EQUAL(PatchError::None, p.addModule(t, 0, cell, &id));
    return id;
}
static void wire(Patch& p, uint16_t f, int fo, uint16_t t, int ti) {
    TEST_ASSERT_EQUAL(PatchError::None, p.addCable(f, fo, t, ti));
}
static double run(Engine& e, int blocks) {
    float buf[kBlock];
    double sum = 0;
    for (int b = 0; b < blocks; b++) {
        e.processBlock(buf);
        e.collect();
        for (int i = 0; i < kBlock; i++) {
            TEST_ASSERT_TRUE(std::isfinite(buf[i]));
            TEST_ASSERT_TRUE(fabsf(buf[i]) <= 1.f);
            sum += (double)(buf[i] * buf[i]);
        }
    }
    return std::sqrt(sum / (blocks * kBlock));
}
static int blocksFor(float sec) { return (int)(sec * kSampleRate / kBlock); }

void test_basic_voice() {
    Patch p;
    uint16_t k = add(p, kKeysType, 0), o = add(p, kOscType, 1), f = add(p, kVcfType, 2),
             a = add(p, kVcaType, 3), e = add(p, kEnvType, 4), out = add(p, kOutType, 5);
    wire(p, k, 0, o, 0);
    wire(p, o, 0, f, 0);
    wire(p, f, 0, a, 0);
    wire(p, k, 1, e, 0);
    wire(p, e, 0, a, 1);
    wire(p, a, 0, out, 0);
    Engine eng(bigHeap);
    TEST_ASSERT_EQUAL(0, eng.apply(p));
    static_cast<KeysModule*>(eng.instance(k))->noteOn(60);
    run(eng, blocksFor(0.5f));  // let the attack settle
    const double rms = run(eng, blocksFor(0.2f));
    TEST_ASSERT_TRUE(rms > 0.01);
    static_cast<KeysModule*>(eng.instance(k))->gateOff();
    run(eng, blocksFor(2.f));  // release is exponential (300 ms): about -40 dB per second
    TEST_ASSERT_TRUE(run(eng, blocksFor(0.1f)) < 0.001);
}

void test_order() {
    Patch p;
    uint16_t out = add(p, kOutType, 0), a = add(p, kVcaType, 1), f = add(p, kVcfType, 2),
             o = add(p, kOscType, 3), k = add(p, kKeysType, 4);
    wire(p, o, 0, f, 0);
    wire(p, f, 0, a, 0);
    wire(p, a, 0, out, 0);
    wire(p, k, 0, o, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    float buf[kBlock];
    eng.processBlock(buf);  // adopt the program
    // order is observable through the published program via a second engine-free compile
    Module* inst[kMaxModules];
    for (int i = 0; i < p.moduleCount(); i++) inst[i] = eng.instance(p.moduleAt(i).id);
    Program* prog = compileProgram(p, inst, 1);
    const uint16_t want[] = {k, o, f, a, out};
    TEST_ASSERT_EQUAL(5, prog->numNodes);
    for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL_PTR(eng.instance(want[i]), prog->nodes[i].m);
    delete prog;
}

void test_feedback_finite() {
    Patch p;
    uint16_t o = add(p, kOscType, 0), d = add(p, kDlyType, 1), f = add(p, kVcfType, 2),
             out = add(p, kOutType, 3);
    wire(p, o, 0, o, 1);
    wire(p, o, 0, d, 0);
    wire(p, d, 0, d, 0);
    wire(p, d, 0, f, 0);
    wire(p, f, 0, out, 0);
    p.find(d)->params[1] = 0.95f;
    p.find(f)->params[1] = 1.f;  // res
    p.find(f)->params[3] = 1.f;  // drive
    Engine eng(bigHeap);
    TEST_ASSERT_EQUAL(0, eng.apply(p));
    run(eng, blocksFor(10.f));  // run() asserts finite and within +-1
}

void test_nan_recovers() {
    Patch p;
    uint16_t n = add(p, kNanType, 0), out = add(p, kOutType, 1);
    wire(p, n, 0, out, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    gNanNow = false;
    run(eng, 50);
    gNanNow = true;
    run(eng, 1);  // asserts finite
    gNanNow = false;
    run(eng, 100);
    TEST_ASSERT_TRUE(run(eng, 20) > 0.01);
}

void test_remove_while_running() {
    Patch p;
    uint16_t o = add(p, kOscType, 0), c = add(p, kCountedType, 1), a = add(p, kVcaType, 2),
             out = add(p, kOutType, 3);
    wire(p, o, 0, c, 0);
    wire(p, c, 0, a, 0);
    wire(p, a, 0, out, 0);
    gDtorCount = 0;
    {
        Engine eng(bigHeap);
        eng.apply(p);
        run(eng, 100);
        p.removeModule(c);
        eng.apply(p);
        TEST_ASSERT_EQUAL(0, gDtorCount);  // the audio thread may still run the old program
        float buf[kBlock];
        eng.collect();
        TEST_ASSERT_EQUAL(0, gDtorCount);  // not acknowledged yet
        eng.processBlock(buf);
        run(eng, 10);
        TEST_ASSERT_EQUAL(1, gDtorCount);
        TEST_ASSERT_NULL(eng.instance(c));
    }
}

void test_amount_inverts() {
    Patch p;
    uint16_t d = add(p, kDcType, 0), s = add(p, kSinkType, 1);
    wire(p, d, 0, s, 0);
    p.find(d)->outAmount[0] = -0.5f;
    Engine eng(bigHeap);
    eng.apply(p);
    run(eng, 3);
    TEST_ASSERT_TRUE(gSinkConnected);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -2.5f, gSinkIn);
    eng.setOutAmount(d, 0, 1.f);
    run(eng, 1);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 5.f, gSinkIn);
}

// ---- review r2: measured costs ----
static uint32_t gClock = 0;
static uint32_t fakeClock() { return gClock += 32 * 100; }  // every module "takes" 100 cycles/sample
static uint32_t tinyBlock() { return 2048; }

void test_measured_cost_replaces_estimate() {
    Engine eng(bigHeap);
    eng.setCycleCounter(fakeClock);
    Patch p;
    add(p, kOscType, 0);
    add(p, kOutType, 1);
    eng.apply(p);
    TEST_ASSERT_EQUAL_UINT32(0, eng.measuredCost(kOscType));
    TEST_ASSERT_EQUAL_UINT32(kOscType.cpuCycles, eng.costOf(kOscType));
    float buf[kBlock];
    for (int i = 0; i < 400; i++) {
        eng.processBlock(buf);
        eng.collect();
    }
    TEST_ASSERT_UINT32_WITHIN(3, 100, eng.measuredCost(kOscType));
    TEST_ASSERT_UINT32_WITHIN(3, 112, eng.costOf(kOscType));  // + 1/8 headroom
    TEST_ASSERT_TRUE(eng.costsChanged());
    TEST_ASSERT_FALSE(eng.costsChanged());
    // A type whose cost varies live never drops below its estimate.
    ModuleTypeInfo varies = kOscType;
    varies.costVaries = true;
    TEST_ASSERT_EQUAL_UINT32(kOscType.cpuCycles, eng.costOf(varies));  // not measured: estimate
}

void test_can_add_live() {
    Engine eng(bigHeap);
    Patch p;
    TEST_ASSERT_TRUE(eng.canAddLive(p, kOscType).ok);
    eng.setCpuLoad(0.79f);  // 3950 of the 5000 cycles: the costs alone would allow it
    TEST_ASSERT_TRUE(eng.canAdd(p, kOscType).ok);
    TEST_ASSERT_EQUAL_STRING("NO CPU", eng.canAddLive(p, kOscType).reason);
    eng.setCpuLoad(0.f);
    eng.setLargestBlock(tinyBlock);  // plenty free, but no single 16 KB block
    ModuleTypeInfo big = kOscType;
    big.largestAlloc = 16384;
    TEST_ASSERT_EQUAL_STRING("NO RAM", eng.canAddLive(p, big).reason);
    TEST_ASSERT_TRUE(eng.canAddLive(p, kOscType).ok);
}

static bool gRefuse = false;
static int gCreates = 0;
static Module* createMaybe() {
    gCreates++;
    return gRefuse ? nullptr : createDcSrc();
}

void test_refused_create_retried_slowly() {
    ModuleTypeInfo flaky = kDcType;
    flaky.create = createMaybe;
    Engine eng(bigHeap);
    Patch p;
    p.addModule(flaky, 0, 0, nullptr);
    gRefuse = true;
    gCreates = 0;
    TEST_ASSERT_EQUAL(1, eng.apply(p));
    TEST_ASSERT_EQUAL(1, eng.missing());
    for (int i = 0; i < 128; i++) eng.collect();
    TEST_ASSERT_TRUE(gCreates <= 3);  // 1 in apply + every 64th collect
    gRefuse = false;
    for (int i = 0; i < 64; i++) eng.collect();
    TEST_ASSERT_EQUAL(0, eng.missing());
}

void test_budget() {
    Patch p;
    Engine small(smallHeap);
    Refusal r = small.canAdd(p, kOscType);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_STRING("NO RAM", r.reason);

    Engine eng(bigHeap);
    TEST_ASSERT_TRUE(eng.canAdd(p, kOscType).ok);
    add(p, kOutType, 0);
    r = eng.canAdd(p, kOutType);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_STRING("MAX 1", r.reason);
    add(p, kKeysType, 1);
    TEST_ASSERT_EQUAL_STRING("MAX 1", eng.canAdd(p, kKeysType).reason);
    add(p, kDlyType, 2);
    TEST_ASSERT_TRUE(eng.canAdd(p, kDlyType).ok);
    add(p, kDlyType, 3);
    r = eng.canAdd(p, kDlyType);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_STRING("NO BUFFER", r.reason);

    // CPU: fill with expensive test modules
    Patch q;
    ModuleTypeInfo heavy = kDcType;
    heavy.cpuCycles = 3000;
    q.addModule(heavy, 0, 0, nullptr);
    r = eng.canAdd(q, heavy);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_STRING("NO CPU", r.reason);
}

void test_load_never_skips_for_ram() {
    // A file's module is never dropped for RAM: it waits in the patch until the heap has room.
    gHeap = 0;
    Engine eng(setHeap);
    Patch p;
    TEST_ASSERT_TRUE(eng.canLoad(p, kOscType).ok);
    const uint16_t o = add(p, kOscType, 0);
    TEST_ASSERT_EQUAL(1, eng.apply(p));  // below the reserve: not created
    TEST_ASSERT_NULL(eng.instance(o));
    gHeap = 1000000;
    for (int i = 0; i < 64; i++) eng.collect();
    TEST_ASSERT_NOT_NULL(eng.instance(o));
    TEST_ASSERT_EQUAL(0, eng.missing());
}

void test_load_skips_no_cpu() {
    // A patch built against the live load must reload whole: only pool slots and MAX 1 skip.
    Engine eng(bigHeap);
    Patch q;
    ModuleTypeInfo heavy = kDcType;
    heavy.cpuCycles = 3000;
    q.addModule(heavy, 0, 0, nullptr);
    TEST_ASSERT_EQUAL_STRING("NO CPU", eng.canAdd(q, heavy).reason);
    TEST_ASSERT_TRUE(eng.canLoad(q, heavy).ok);
    add(q, kDlyType, 1);
    add(q, kDlyType, 2);
    TEST_ASSERT_EQUAL_STRING("NO BUFFER", eng.canLoad(q, kDlyType).reason);  // only two exist
}

void test_live_cpu_is_measured_load() {
    // The per-type sum (each type's worst moment) refuses; the measured load has room.
    Engine eng(bigHeap);
    Patch q;
    ModuleTypeInfo heavy = kDcType;
    heavy.cpuCycles = 2500;
    q.addModule(heavy, 0, 0, nullptr);
    TEST_ASSERT_EQUAL_STRING("NO CPU", eng.canAdd(q, heavy).reason);
    eng.setCpuLoad(0.2f);  // 1000 cycles measured
    TEST_ASSERT_TRUE(eng.canAddLive(q, heavy).ok);
    eng.setCpuLoad(0.4f);
    TEST_ASSERT_EQUAL_STRING("NO CPU", eng.canAddLive(q, heavy).reason);
}

void test_replaced_dly_waits_for_static_slot() {
    // A new DLY must not take the 96 KB heap slot while its retired predecessor still holds the
    // static one: it is missing for one collect(), then lands in the static slot.
    Engine eng(bigHeap);
    Patch a;
    add(a, kDlyType, 0);
    TEST_ASSERT_EQUAL(0, eng.apply(a));
    run(eng, 2);
    Patch b;
    b.nextId = 50;
    const uint16_t d = add(b, kDlyType, 0);
    TEST_ASSERT_EQUAL(1, eng.apply(b));
    TEST_ASSERT_NULL(eng.instance(d));
    run(eng, 3);
    TEST_ASSERT_NOT_NULL(eng.instance(d));
    TEST_ASSERT_EQUAL_INT(0, delayPoolStaticFree());
    TEST_ASSERT_EQUAL_INT(1, delayPoolFree());  // the heap slot was never taken
    eng.apply(Patch());
    run(eng, 3);
}

void test_second_delay_slot_is_heap() {
    // The first DLY/REV takes the static slot; the second needs a 96 KB heap block.
    gHeap = kRamReserve + 50 * 1024;
    Engine eng(setHeap);
    Patch p;
    TEST_ASSERT_TRUE(eng.canAdd(p, kDlyType).ok);
    add(p, kDlyType, 0);
    TEST_ASSERT_EQUAL_STRING("NO RAM", eng.canAdd(p, kDlyType).reason);
    gHeap = kRamReserve + kDelaySlotBytes + 1024;
    TEST_ASSERT_TRUE(eng.canAdd(p, kDlyType).ok);
    eng.setLargestBlock(tinyBlock);
    TEST_ASSERT_EQUAL_STRING("NO RAM", eng.canAddLive(p, kDlyType).reason);
    TEST_ASSERT_EQUAL_UINT32(0, delayPoolHeapBytes(0, 1));
    TEST_ASSERT_EQUAL_UINT32(kDelaySlotBytes, delayPoolHeapBytes(1, 1));
}

void test_apply_param_change_keeps_state() {
    Patch p;
    uint16_t o = add(p, kOscType, 0), out = add(p, kOutType, 1);
    wire(p, o, 0, out, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    Module* before = eng.instance(o);
    run(eng, 10);
    p.find(o)->params[0] = 1.f;
    eng.apply(p);
    TEST_ASSERT_EQUAL_PTR(before, eng.instance(o));
    TEST_ASSERT_EQUAL_FLOAT(1.f, before->param(0));
}

void test_dly_replacement_retries() {
    Patch a;
    add(a, kDlyType, 0);
    add(a, kDlyType, 1);
    Engine eng(bigHeap);
    TEST_ASSERT_EQUAL(0, eng.apply(a));
    run(eng, 5);
    Patch b;
    b.nextId = 100;
    uint16_t d1 = add(b, kDlyType, 0), d2 = add(b, kDlyType, 1);
    TEST_ASSERT_EQUAL(2, eng.apply(b));  // old DLYs still hold the pool
    TEST_ASSERT_NULL(eng.instance(d1));
    float buf[kBlock];
    eng.processBlock(buf);
    eng.collect();
    TEST_ASSERT_NOT_NULL(eng.instance(d1));
    TEST_ASSERT_NOT_NULL(eng.instance(d2));
    run(eng, 5);  // the republished program is adopted and the old one freed
    TEST_ASSERT_EQUAL_INT(0, delayPoolFree());
}

void test_type_change_recreates() {
    Patch p;
    uint16_t id = add(p, kVcaType, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    Module* before = eng.instance(id);
    Patch q;  // same id, other type
    q.addModule(kVcfType, 0, 0, nullptr);
    TEST_ASSERT_EQUAL_UINT16(id, q.moduleAt(0).id);
    eng.apply(q);
    TEST_ASSERT_TRUE(eng.instance(id) != before);
    TEST_ASSERT_EQUAL_PTR(&kVcfType, &eng.instance(id)->type());
    run(eng, 5);
}

void test_order_cycle_downstream() {
    Patch p;
    uint16_t out = add(p, kOutType, 0), o = add(p, kOscType, 1), f = add(p, kVcfType, 2),
             a = add(p, kVcaType, 3);
    wire(p, o, 0, f, 0);
    wire(p, f, 0, a, 0);
    wire(p, a, 0, o, 1);  // loop OSC -> VCF -> VCA -> OSC
    wire(p, a, 0, out, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    Module* inst[kMaxModules];
    for (int i = 0; i < p.moduleCount(); i++) inst[i] = eng.instance(p.moduleAt(i).id);
    Program* prog = compileProgram(p, inst, 1);
    TEST_ASSERT_EQUAL(4, prog->numNodes);
    int posOut = -1, posVca = -1;
    for (int i = 0; i < 4; i++) {
        if (prog->nodes[i].m == eng.instance(out)) posOut = i;
        if (prog->nodes[i].m == eng.instance(a)) posVca = i;
    }
    TEST_ASSERT_TRUE(posOut > posVca);
    TEST_ASSERT_EQUAL_PTR(eng.instance(o), prog->nodes[0].m);  // loop broken at the lowest index inside it
    delete prog;
}

void test_double_apply_before_block() {
    Patch p;
    uint16_t c = add(p, kCountedType, 0), out = add(p, kOutType, 1);
    wire(p, c, 0, out, 0);
    gDtorCount = 0;
    {
        Engine eng(bigHeap);
        eng.apply(p);  // program 1, never run
        Patch q = p;
        q.removeModule(c);
        eng.apply(q);  // program 2, never run either
        eng.collect();
        TEST_ASSERT_EQUAL(0, gDtorCount);
        float buf[kBlock];
        eng.processBlock(buf);
        eng.collect();
        TEST_ASSERT_EQUAL(1, gDtorCount);  // both old programs and the module freed
    }
}

void test_apply_collect_without_block() {
    Patch p;
    uint16_t c = add(p, kCountedType, 0), out = add(p, kOutType, 1);
    wire(p, c, 0, out, 0);
    gDtorCount = 0;
    Engine eng(bigHeap);
    eng.apply(p);
    run(eng, 5);
    Patch q = p;
    q.removeModule(c);
    eng.apply(q);
    for (int i = 0; i < 3; i++) eng.collect();
    TEST_ASSERT_EQUAL(0, gDtorCount);
    run(eng, 2);
    TEST_ASSERT_EQUAL(1, gDtorCount);
}


void test_input_amount_scales_sum() {
    Patch p;
    uint16_t d1 = add(p, kDcType, 0), d2 = add(p, kDcType, 2), s = add(p, kSinkType, 1);
    wire(p, d1, 0, s, 0);
    wire(p, d2, 0, s, 0);
    p.find(s)->inAmount[0] = -0.5f;
    Engine eng(bigHeap);
    eng.apply(p);
    run(eng, 2);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -5.f, gSinkIn);  // (5 + 5) x -0.5
    eng.setInAmount(s, 0, 0.25f);
    run(eng, 1);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.5f, gSinkIn);
}

void test_input_amount_default_one() {
    Patch p;
    uint16_t s = add(p, kSinkType, 1);
    for (int i = 0; i < kMaxIns; i++) TEST_ASSERT_EQUAL_FLOAT(1.f, p.find(s)->inAmount[i]);
}

void test_peak_meter() {
    Patch p;
    uint16_t d = add(p, kDcType, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    run(eng, 1);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 5.f, eng.instance(d)->peak());
}

// ---- v1.3 bypass ----
// A bypassed effect is not processed and passes input 0 to output 0 (with its input amount).
void test_bypass_passes_effect() {
    Patch p;
    const uint16_t d = add(p, kDcType, 0), f = add(p, kVcfType, 1), s = add(p, kSinkType, 2);
    wire(p, d, 0, f, 0);
    wire(p, f, 0, s, 0);
    p.find(f)->inAmount[0] = 0.5f;
    Engine eng(bigHeap);
    eng.apply(p);  // enabled first: the real filter exists, then bypassed it stays
    p.setBypass(f, true);
    eng.setCycleCounter(fakeClock);
    eng.apply(p);
    run(eng, 4);
    TEST_ASSERT_TRUE(bypassPasses(kVcfType));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, gSinkIn);  // 5 V x 0.5: the filter did not run
    TEST_ASSERT_EQUAL(0, eng.instance(f)->measuredCycles());
    TEST_ASSERT_EQUAL_FLOAT(2.5f, eng.instance(f)->peak());
}

// Bypassed before it ever ran (a load, an undo): no real instance and no heap, only a stub that
// still passes audio; enabling creates the real module. RAM short: it waits, missing.
void test_bypassed_module_not_created() {
    Patch p;
    const uint16_t d = add(p, kDcType, 0), f = add(p, kVcfType, 1), s = add(p, kSinkType, 2);
    uint16_t g = 0;
    p.addModule(kGrdType, 0, 3, &g);
    wire(p, d, 0, f, 0);
    wire(p, f, 0, s, 0);
    p.setBypass(f, true);
    p.setBypass(g, true);
    gHeap = 70 * 1024;  // a GRD would fit (26 KB + the 32 KB reserve), but bypassed it is not made
    Engine eng(setHeap);
    TEST_ASSERT_EQUAL(0, eng.apply(p));
    run(eng, 2);
    TEST_ASSERT_NULL(eng.instance(f));
    TEST_ASSERT_NULL(eng.instance(g));
    TEST_ASSERT_EQUAL(0, eng.missing());
    TEST_ASSERT_EQUAL_FLOAT(5.f, gSinkIn);  // the stub passes input 0
    p.setBypass(f, false);
    TEST_ASSERT_EQUAL(0, eng.apply(p));
    run(eng, 2);
    TEST_ASSERT_NOT_NULL(eng.instance(f));  // the real filter now
    gHeap = 40 * 1024;
    p.setBypass(g, false);
    TEST_ASSERT_EQUAL(1, eng.apply(p));  // enabled without the RAM: missing, retried by collect()
    TEST_ASSERT_NULL(eng.instance(g));
    gHeap = 1000000;
    for (int i = 0; i < 70; i++) {
        run(eng, 1);
        eng.collect();
    }
    TEST_ASSERT_NOT_NULL(eng.instance(g));
    TEST_ASSERT_EQUAL(0, eng.missing());
    gHeap = 0;
}

// Any other bypassed module outputs 0 (not the last block it computed) and the instance stays.
void test_bypass_zeroes_others() {
    Patch p;
    const uint16_t d = add(p, kDcType, 0), s = add(p, kSinkType, 1);
    wire(p, d, 0, s, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    run(eng, 2);
    TEST_ASSERT_EQUAL_FLOAT(5.f, gSinkIn);
    Module* before = eng.instance(d);
    p.setBypass(d, true);
    eng.apply(p);
    run(eng, 2);
    TEST_ASSERT_FALSE(bypassPasses(kDcType));
    TEST_ASSERT_EQUAL_FLOAT(0.f, gSinkIn);
    TEST_ASSERT_TRUE(eng.instance(d) == before);  // kept: instant toggle, no RAM churn
    p.setBypass(d, false);
    eng.apply(p);
    run(eng, 2);
    TEST_ASSERT_EQUAL_FLOAT(5.f, gSinkIn);
}

// A bypassed OUT adds nothing to the master; a bypassed module is not in the CPU sums.
void test_bypass_out_silent_and_free() {
    Patch p;
    uint16_t k = add(p, kKeysType, 0), o = add(p, kOscType, 1), out = add(p, kOutType, 2);
    wire(p, k, 0, o, 0);
    wire(p, o, 0, out, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    TEST_ASSERT_TRUE(run(eng, 20) > 0.01);
    p.setBypass(out, true);
    eng.apply(p);
    run(eng, 1);
    TEST_ASSERT_TRUE(run(eng, 10) == 0.0);

    Patch q;
    ModuleTypeInfo heavy = kDcType;
    heavy.cpuCycles = 3000;
    uint16_t h = 0;
    q.addModule(heavy, 0, 0, &h);
    TEST_ASSERT_FALSE(eng.canAdd(q, heavy).ok);
    q.setBypass(h, true);
    TEST_ASSERT_TRUE(eng.canAdd(q, heavy).ok);
    eng.apply(q);
    TEST_ASSERT_EQUAL(0, eng.estimatedCycles());
}


// ---- KIT: demand-driven activation ----
// Burst: on a rising input 0, outputs 1 V for gBurstLen samples (forever when < 0); counts its
// process() calls. Heavy: a 1-input passthrough with a big estimated cost.
static int gBurstLen = 480;
static int gBurstCalls = 0;
class Burst : public Module {
   public:
    Burst();
    void process(const float* const* ins, const bool* c) override {
        gBurstCalls++;
        for (int i = 0; i < kBlock; i++) {
            if (gd_.process(c[0] ? ins[0][i] : 0.f) && gd_.rose) left_ = gBurstLen < 0 ? INT32_MAX : gBurstLen;
            out(0)[i] = left_ > 0 ? 1.f : 0.f;
            if (left_ > 0 && left_ != INT32_MAX) left_--;
        }
    }
    GateDetector gd_;
    int left_ = 0;
};
static Module* createBurst() { return new Burst(); }
const ModuleTypeInfo kBurstType = {"tburst", "TST", "test", Category::Processor, Icon::Osc, Drawing::None,
                                   0, nullptr, 1, kTIn, kTIn, 1, kTOut, kTOut, 10, 100, 0, createBurst};
Burst::Burst() : Module(kBurstType) {}
extern const ModuleTypeInfo kHeavyType;
class Heavy : public Module {
   public:
    Heavy() : Module(kHeavyType) {}
    void process(const float* const* ins, const bool* c) override {
        for (int i = 0; i < kBlock; i++) out(0)[i] = c[0] ? ins[0][i] : 0.f;
    }
};
static Module* createHeavy() { return new Heavy(); }
const ModuleTypeInfo kHeavyType = {"theavy", "TST", "test", Category::Processor, Icon::Osc, Drawing::None,
                                   0, nullptr, 1, kTIn, kTIn, 1, kTOut, kTOut, 1500, 100, 0, createHeavy};

void test_kit_groups() {
    Patch p;
    const uint16_t kit = add(p, kKitType, 0), a = add(p, kBurstType, 1), b = add(p, kBurstType, 2),
                   mix = add(p, kMixType, 3), out = add(p, kOutType, 4), lfo = add(p, kLfoType, 5),
                   both = add(p, kBurstType, 6), accd = add(p, kKckType, 7), c1 = add(p, kBurstType, 8),
                   c2 = add(p, kBurstType, 9), lone = add(p, kBurstType, 10), direct = add(p, kBurstType, 11);
    wire(p, kit, 0, a, 0);           // a: T1
    wire(p, a, 0, b, 0);             // b: fed only by a -> T1
    wire(p, kit, 1, mix, 0);         // mix: T2 and b -> shared
    wire(p, b, 0, mix, 1);
    wire(p, mix, 0, out, 0);
    wire(p, kit, 2, both, 0);        // both: T3 and the LFO -> shared
    wire(p, lfo, 0, both, 0);
    wire(p, kit, 3, accd, 0);        // accd: T4 and the KIT's ACC (neutral) -> T4
    wire(p, kit, kit::kAccOut, accd, 1);
    wire(p, kit, 4, c1, 0);          // c1 <-> c2, both from T5: a cycle stays always-on
    wire(p, c2, 0, c1, 0);
    wire(p, c1, 0, c2, 0);
    wire(p, kit, 5, direct, 0);      // direct: T6 and a self-cable -> T6
    wire(p, direct, 0, direct, 0);
    wire(p, a, 0, out, 0);           // OUT: never grouped
    (void)lone;                      // lone: no inputs -> always-on
    uint8_t g[kMaxModules];
    TEST_ASSERT_EQUAL(0, kitGroups(p, g));
    auto grp = [&](uint16_t id) {
        for (int i = 0; i < p.moduleCount(); i++)
            if (p.moduleAt(i).id == id) return (int)g[i];
        return -2;
    };
    TEST_ASSERT_EQUAL(kNoGroup, grp(kit));
    TEST_ASSERT_EQUAL(0, grp(a));
    TEST_ASSERT_EQUAL(0, grp(b));
    TEST_ASSERT_EQUAL(kNoGroup, grp(mix));
    TEST_ASSERT_EQUAL(kNoGroup, grp(out));
    TEST_ASSERT_EQUAL(kNoGroup, grp(lfo));
    TEST_ASSERT_EQUAL(kNoGroup, grp(both));
    TEST_ASSERT_EQUAL(3, grp(accd));
    TEST_ASSERT_EQUAL(kNoGroup, grp(c1));
    TEST_ASSERT_EQUAL(kNoGroup, grp(c2));
    TEST_ASSERT_EQUAL(kNoGroup, grp(lone));
    TEST_ASSERT_EQUAL(5, grp(direct));
    // a source without an instance is ignored: with the LFO missing, `both` is T3's
    bool present[kMaxModules];
    for (int i = 0; i < p.moduleCount(); i++) present[i] = p.moduleAt(i).id != lfo;
    kitGroups(p, g, present);
    TEST_ASSERT_EQUAL(2, grp(both));
    // review I2: a delay's echo outlives the 50 ms quiet window, and a held CV drives an
    // always-on module: neither runs on demand. An envelope inside a group still does.
    Patch d;
    const uint16_t dk = add(d, kKitType, 0), dkick = add(d, kKckType, 1), ddly = add(d, kDlyType, 2),
                   dout = add(d, kOutType, 3), dseq = add(d, kSeqType, 4), dosc = add(d, kOscType, 5),
                   denv = add(d, kEnvType, 6), dsnr = add(d, kSnrType, 7);
    wire(d, dk, 0, dkick, 0);
    wire(d, dkick, 0, ddly, 0);
    wire(d, ddly, 0, dout, 0);
    wire(d, dk, 1, dseq, 0);   // T2 clocks a SEQ that tunes an always-on OSC
    wire(d, dseq, 0, dosc, 0);
    wire(d, dosc, 0, dout, 0);
    wire(d, dk, 2, denv, 0);   // T3 -> ENV -> SNR's accent, SNR triggered by T3: all T3's
    wire(d, denv, 0, dsnr, 1);
    wire(d, dk, 2, dsnr, 0);
    wire(d, dsnr, 0, dout, 0);
    kitGroups(d, g);
    auto dg = [&](uint16_t id) {
        for (int i = 0; i < d.moduleCount(); i++)
            if (d.moduleAt(i).id == id) return (int)g[i];
        return -2;
    };
    TEST_ASSERT_EQUAL(0, dg(dkick));
    TEST_ASSERT_EQUAL(kNoGroup, dg(ddly));
    TEST_ASSERT_EQUAL(kNoGroup, dg(dseq));
    TEST_ASSERT_EQUAL(kNoGroup, dg(dosc));  // tuned by the lane, never struck: it never rests
    // final review M1: a voice's CV outs (GRD env/lfo) are no audio: driving an always-on module,
    // the voice runs always
    Patch v;
    const uint16_t vk = add(v, kKitType, 0), vg = add(v, kGrdType, 1), vo = add(v, kOscType, 2),
                   vx = add(v, kOutType, 3);
    wire(v, vk, 0, vg, 1);
    wire(v, vg, 3, vo, 1);  // GRD lfo -> OSC fm
    wire(v, vo, 0, vx, 0);
    wire(v, vg, 0, vx, 0);
    kitGroups(v, g);
    TEST_ASSERT_EQUAL(kNoGroup, g[1]);
    v.removeCablesAtPin(vg, true, 3);
    kitGroups(v, g);
    TEST_ASSERT_EQUAL(0, g[1]);
    TEST_ASSERT_EQUAL(2, dg(denv));
    TEST_ASSERT_EQUAL(2, dg(dsnr));
    // no KIT: nothing is grouped
    Patch q;
    add(q, kCountedType, 0);
    TEST_ASSERT_EQUAL(-1, kitGroups(q, g));
    TEST_ASSERT_EQUAL(kNoGroup, g[0]);
}

// KIT T1 -> burst A -> OUT, T2 -> burst B -> OUT. A group runs only from its trigger until 50 ms
// of quiet or the next step.
void test_kit_demand_activation() {
    Patch p;
    const uint16_t kit = add(p, kKitType, 0), a = add(p, kBurstType, 1), out = add(p, kOutType, 2);
    wire(p, kit, 0, a, 0);
    wire(p, a, 0, out, 0);
    p.find(kit)->params[kit::kLen] = 2.f;
    p.find(kit)->params[kit::kGap] = 500.f;  // 24000 samples, 750 blocks
    Engine eng(bigHeap);
    TEST_ASSERT_EQUAL(0, eng.apply(p));
    gBurstLen = 480;  // 10 ms
    gBurstCalls = 0;
    run(eng, 100);
    TEST_ASSERT_EQUAL(0, gBurstCalls);  // KIT idle: the group costs nothing
    static_cast<KitModule*>(eng.instance(kit))->start();
    run(eng, 1);
    TEST_ASSERT_EQUAL(1, gBurstCalls);  // the trigger's block runs it
    TEST_ASSERT_EQUAL_FLOAT(1.f, eng.instance(a)->out(0)[0]);
    // 10 ms of sound, then 50 ms of quiet: off at about 60 ms (2880 samples = 90 blocks)
    run(eng, 200);
    TEST_ASSERT_TRUE(gBurstCalls >= 89 && gBurstCalls <= 92);
    for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(0.f, eng.instance(a)->out(0)[i]);
    // a sound that never stops is cut at the next step (block 750), and its outputs read 0
    gBurstLen = -1;
    gBurstCalls = 0;
    static_cast<KitModule*>(eng.instance(kit))->start();
    run(eng, 750);  // the next step starts in block 750
    TEST_ASSERT_EQUAL_FLOAT(1.f, eng.instance(a)->out(0)[kBlock - 1]);
    // the cut fades out over 4 blocks (no click), then the group stops; the 4th block ramps to 0
    // too, but its buffer is zeroed once it has been read
    float last = 1.f;
    for (int b = 0; b < 3; b++) {
        run(eng, 1);
        const float* o = eng.instance(a)->out(0);
        for (int i = 0; i < kBlock; i++) {
            TEST_ASSERT_TRUE(o[i] <= last + 1e-6f);
            TEST_ASSERT_TRUE(last - o[i] < 0.02f);  // a ramp, not a step
            last = o[i];
        }
    }
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.25f, last);
    run(eng, 8);
    TEST_ASSERT_EQUAL(754, gBurstCalls);
    for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(0.f, eng.instance(a)->out(0)[i]);
    // the pass is over: nothing restarts it
    const int calls = gBurstCalls;
    run(eng, 800);
    TEST_ASSERT_EQUAL(calls, gBurstCalls);
    gBurstLen = 480;
}

// The guard: always-on modules + the worst single step's groups, not every group.
void test_kit_cpu_guard() {
    Patch p;
    const uint16_t kit = add(p, kKitType, 0), out = add(p, kOutType, 1), mix = add(p, kMixType, 2);
    uint16_t h[3];
    for (int i = 0; i < 3; i++) {
        h[i] = add(p, kHeavyType, 3 + i);
        wire(p, kit, i, h[i], 0);  // default lanes: T1 plays step 1, T2 step 2, T3 step 3
        wire(p, h[i], 0, mix, i);
    }
    wire(p, mix, 0, out, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    const uint32_t base = kKitType.cpuCycles + kOutType.cpuCycles + kMixType.cpuCycles;
    TEST_ASSERT_EQUAL_UINT32(base + 1500, eng.estimatedCycles());
    TEST_ASSERT_TRUE(eng.canAdd(p, kHeavyType).ok);  // an unpatched one runs always: base + 3000
    // stacked: step 1 fires T1, T2 and T3 at once
    p.find(kit)->params[kit::kL1 + 1] = (float)0b011;
    p.find(kit)->params[kit::kL1 + 2] = (float)0b101;
    eng.apply(p);
    TEST_ASSERT_EQUAL_UINT32(base + 4500, eng.estimatedCycles());
    TEST_ASSERT_FALSE(eng.canAdd(p, kHeavyType).ok);
    // the live check adds the worst step to the measured load (an idle KIT measures nothing)
    eng.setCpuLoad(0.1f);  // 500 cycles
    TEST_ASSERT_FALSE(eng.canAddLive(p, kCountedType).ok);
    p.find(kit)->params[kit::kL1 + 1] = (float)0b010;
    p.find(kit)->params[kit::kL1 + 2] = (float)0b100;
    TEST_ASSERT_TRUE(eng.canAddLive(p, kCountedType).ok);
    // a bypassed KIT fires nothing: its groups cost nothing
    p.setBypass(kit, true);
    eng.apply(p);
    TEST_ASSERT_EQUAL_UINT32(kOutType.cpuCycles + kMixType.cpuCycles, eng.estimatedCycles());
}

// Task 9: a single source at unity amounts is read in place (no copy); a self-cable, a second
// source or an amount still go through the input buffer.
static const float* gSeenIn = nullptr;
class Peek : public Module {
   public:
    Peek();
    void process(const float* const* ins, const bool* c) override {
        gSeenIn = c[0] ? ins[0] : nullptr;
        for (int i = 0; i < kBlock; i++) out(0)[i] = c[0] ? ins[0][i] + 1.f : 0.f;
    }
};
static Module* createPeek() { return new Peek(); }
const ModuleTypeInfo kPeekType = {"tpeek", "TST", "test", Category::Processor, Icon::Osc, Drawing::None,
                                  0, nullptr, 1, kTIn, kTIn, 1, kTOut, kTOut, 10, 100, 0, createPeek};
Peek::Peek() : Module(kPeekType) {}

void test_input_read_in_place() {
    Patch p;
    const uint16_t src = add(p, kDcType, 0), pk = add(p, kPeekType, 1);
    wire(p, src, 0, pk, 0);
    Engine eng(bigHeap);
    eng.apply(p);
    float buf[kBlock];
    eng.processBlock(buf);
    TEST_ASSERT_TRUE(gSeenIn == eng.instance(src)->out(0));
    TEST_ASSERT_EQUAL_FLOAT(6.f, eng.instance(pk)->out(0)[5]);
    p.find(src)->outAmount[0] = 0.5f;  // an amount: scaled in the buffer
    eng.apply(p);
    eng.processBlock(buf);
    TEST_ASSERT_TRUE(gSeenIn != eng.instance(src)->out(0));
    TEST_ASSERT_EQUAL_FLOAT(3.5f, eng.instance(pk)->out(0)[5]);
    // a self-cable reads the previous block, never the buffer being written
    Patch q;
    const uint16_t s = add(q, kPeekType, 0);
    wire(q, s, 0, s, 0);
    Engine e2(bigHeap);
    e2.apply(q);
    for (int b = 0; b < 3; b++) e2.processBlock(buf);
    TEST_ASSERT_TRUE(gSeenIn != e2.instance(s)->out(0));
    TEST_ASSERT_EQUAL_FLOAT(3.f, e2.instance(s)->out(0)[0]);
}

// Review I1: a voice cut while ringing starts its next hit fresh, as the first one did.
void test_kit_cut_voice_restarts_clean() {
    const ModuleTypeInfo* voices[] = {&kResType, &kStrType, &kMdlType, &kKckType, &kGrdType};
    for (const ModuleTypeInfo* vt : voices) {
        Patch p;
        const uint16_t kit = add(p, kKitType, 0), v = add(p, *vt, 1), out = add(p, kOutType, 2);
        int strike = 0;  // the trig (or audio in) pin
        while (strcmp(vt->inKeys[strike], "trig") && strcmp(vt->inKeys[strike], "in")) strike++;
        wire(p, kit, 0, v, strike);
        if (vt == &kGrdType) {  // PLTKIT's Plaits bass drum (review: its tail outlived the cut)
            float* g = p.find(v)->params;
            g[0] = 13.f;   // eng BD
            g[1] = -1.f;   // oct
            g[9] = 0.6f;   // decay
            g[11] = 1.f;   // vca LPG
            g[12] = 2.f;   // flt OFF
        }
        wire(p, v, 0, out, 0);
        p.find(kit)->params[kit::kL1] = (float)0b101;  // T1 on steps 1 and 3; step 2 cuts it
        p.find(kit)->params[kit::kLen] = 3.f;
        p.find(kit)->params[kit::kGap] = 150.f;  // 225 blocks
        Engine eng(bigHeap);
        TEST_ASSERT_EQUAL(0, eng.apply(p));
        static_cast<KitModule*>(eng.instance(kit))->start();
        float buf[kBlock], first[20 * kBlock];
        double diff = 0, e1 = 0, e2 = 0;
        for (int b = 0; b < 470; b++) {
            eng.processBlock(buf);
            const float* o = eng.instance(v)->out(0);
            for (int i = 0; i < kBlock; i++) {
                if (b < 20) first[b * kBlock + i] = o[i];
                if (b < 20) e1 += (double)o[i] * o[i];
                if (b >= 450) {
                    diff = fmax(diff, fabs((double)o[i] - first[(b - 450) * kBlock + i]));
                    e2 += (double)o[i] * o[i];
                }
            }
        }
        if (vt == &kStrType)  // a noise-burst pluck: the same energy, not the same samples
            TEST_ASSERT_TRUE_MESSAGE(e2 > 0.8 * e1 && e2 < 1.25 * e1, vt->id);
        else
            TEST_ASSERT_TRUE_MESSAGE(diff < 1e-3, vt->id);
    }
}

// Review M6: a cycle through the KIT (T1 -> X -> KIT RUN) puts X first: it runs always.
void test_kit_node_before_kit_is_always_on() {
    Patch p;
    const uint16_t env = add(p, kBurstType, 0), kit = add(p, kKitType, 1);
    wire(p, kit, 0, env, 0);
    wire(p, env, 0, kit, 0);
    Module* inst[kMaxModules] = {kBurstType.create(), kKitType.create()};
    Program* prog = compileProgram(p, inst, 1);
    TEST_ASSERT_EQUAL(1, prog->kitNode);
    TEST_ASSERT_EQUAL(kNoGroup, prog->nodes[0].group);
    delete prog;
    delete inst[0];
    delete inst[1];
}

// A 3-drum kit of heavy voices: over budget summed, under it grouped; and it plays.
void test_kit_three_drum_kit() {
    Patch p;
    const uint16_t kit = add(p, kKitType, 0), mix = add(p, kMixType, 1), out = add(p, kOutType, 2);
    const ModuleTypeInfo* voices[] = {&kMdlType, &kResType, &kStrType, &kKckType, &kSnrType, &kHatType};
    uint32_t sum = 0, worst = 0;
    for (int i = 0; i < 6; i++) {
        const uint16_t v = add(p, *voices[i], 3 + i);
        wire(p, kit, i, v, 0);
        wire(p, v, 0, mix, i % 4);
        sum += voices[i]->cpuCycles;
        worst = voices[i]->cpuCycles > worst ? voices[i]->cpuCycles : worst;
    }
    wire(p, mix, 0, out, 0);
    p.find(kit)->params[kit::kLen] = 6.f;
    p.find(kit)->params[kit::kGap] = 150.f;
    Engine eng(bigHeap);
    TEST_ASSERT_EQUAL(0, eng.apply(p));
    const uint32_t base = kKitType.cpuCycles + kMixType.cpuCycles + kOutType.cpuCycles;
    TEST_ASSERT_EQUAL_UINT32(base + worst, eng.estimatedCycles());
    TEST_ASSERT_TRUE(base + worst <= kCpuBudget);
    TEST_ASSERT_TRUE(base + sum > kCpuBudget);  // summed, the guard would refuse it
    static_cast<KitModule*>(eng.instance(kit))->start();
    const int stepBlocks = 7200 / kBlock;
    for (int s = 0; s < 6; s++) TEST_ASSERT_TRUE_MESSAGE(run(eng, stepBlocks) > 0.001, voices[s]->id);
    // pass over: every voice idle at 0; OUT only has its DC blocker settling (SNR's DC offset)
    run(eng, 4);
    for (int i = 0; i < 6; i++) {
        const Module* v = eng.instance(p.moduleAt(3 + i).id);
        for (int k = 0; k < kBlock; k++) TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.f, const_cast<Module*>(v)->out(0)[k], voices[i]->id);
    }
    TEST_ASSERT_TRUE(run(eng, 100) < 0.003);  // -50 dBFS
}

// KEYS bypassed still gets its real instance; stubs need heap above half the reserve; collect()
// retries a bypassed record as a stub, never the real module.
void test_bypass_stub_rules() {
    Patch p;
    const uint16_t k = add(p, kKeysType, 0);
    uint16_t g = 0;
    p.addModule(kGrdType, 0, 1, &g);
    p.setBypass(k, true);
    p.setBypass(g, true);
    gHeap = 10 * 1024;  // below kRamReserve / 2: no stub yet
    Engine eng(setHeap);
    eng.apply(p);
    TEST_ASSERT_EQUAL(2, eng.missing());  // the GRD's stub and the (real) KEYS both wait
    gHeap = 1000000;
    Engine big(bigHeap);
    big.apply(p);
    TEST_ASSERT_NOT_NULL(big.instance(k));  // KEYS: real even when bypassed
    TEST_ASSERT_NULL(big.instance(g));
    for (int i = 0; i < 70; i++) {  // heap back: collect() makes the GRD a stub, not a GRD
        run(eng, 1);
        eng.collect();
    }
    TEST_ASSERT_EQUAL(0, eng.missing());
    TEST_ASSERT_NULL(eng.instance(g));
    TEST_ASSERT_NOT_NULL(eng.instance(k));
    gHeap = 0;
}

// Which types pass audio through when bypassed (the report lists them).
void test_bypass_pass_types() {
    const char* const pass[] = {"vcf", "vca", "dly", "rev", "cho", "flg", "pha", "trm", "psh", "lad",
                                "fld", "ovd", "cru", "lpg", "dyn", "awh", "res", "vow", "eq3"};
    int n = 0, count = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    for (int i = 0; i < n; i++) {
        bool want = false;
        for (const char* id : pass) want |= strcmp(id, t[i]->id) == 0;
        TEST_ASSERT_EQUAL_MESSAGE(want, bypassPasses(*t[i]), t[i]->id);
        count += want;
    }
    TEST_ASSERT_EQUAL(19, count);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_basic_voice);
    RUN_TEST(test_bypass_passes_effect);
    RUN_TEST(test_bypassed_module_not_created);
    RUN_TEST(test_bypass_stub_rules);
    RUN_TEST(test_kit_groups);
    RUN_TEST(test_input_read_in_place);
    RUN_TEST(test_kit_demand_activation);
    RUN_TEST(test_kit_cpu_guard);
    RUN_TEST(test_kit_three_drum_kit);
    RUN_TEST(test_kit_cut_voice_restarts_clean);
    RUN_TEST(test_kit_node_before_kit_is_always_on);
    RUN_TEST(test_bypass_zeroes_others);
    RUN_TEST(test_bypass_out_silent_and_free);
    RUN_TEST(test_bypass_pass_types);
    RUN_TEST(test_order);
    RUN_TEST(test_feedback_finite);
    RUN_TEST(test_nan_recovers);
    RUN_TEST(test_remove_while_running);
    RUN_TEST(test_amount_inverts);
    RUN_TEST(test_budget);
    RUN_TEST(test_measured_cost_replaces_estimate);
    RUN_TEST(test_can_add_live);
    RUN_TEST(test_refused_create_retried_slowly);
    RUN_TEST(test_apply_param_change_keeps_state);
    RUN_TEST(test_dly_replacement_retries);
    RUN_TEST(test_type_change_recreates);
    RUN_TEST(test_order_cycle_downstream);
    RUN_TEST(test_double_apply_before_block);
    RUN_TEST(test_apply_collect_without_block);
    RUN_TEST(test_input_amount_scales_sum);
    RUN_TEST(test_input_amount_default_one);
    RUN_TEST(test_peak_meter);
    RUN_TEST(test_load_never_skips_for_ram);
    RUN_TEST(test_load_skips_no_cpu);
    RUN_TEST(test_live_cpu_is_measured_load);
    RUN_TEST(test_replaced_dly_waits_for_static_slot);
    RUN_TEST(test_second_delay_slot_is_heap);
    return UNITY_END();
}
