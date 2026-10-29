#include <unity.h>
#include <cmath>
#include "core/delay_pool.h"
#include "core/engine.h"
#include "core/registry.h"
#include <cstring>
#include "core/modules/dly.h"
#include "core/modules/env.h"
#include "core/modules/keys.h"
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
    p.setBypass(f, true);
    Engine eng(bigHeap);
    eng.setCycleCounter(fakeClock);
    eng.apply(p);
    run(eng, 4);
    TEST_ASSERT_TRUE(bypassPasses(kVcfType));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, gSinkIn);  // 5 V x 0.5: the filter did not run
    TEST_ASSERT_EQUAL(0, eng.instance(f)->measuredCycles());
    TEST_ASSERT_EQUAL_FLOAT(2.5f, eng.instance(f)->peak());
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

// Which types pass audio through when bypassed (the report lists them).
void test_bypass_pass_types() {
    const char* const pass[] = {"vcf", "vca", "dly", "rev", "cho", "flg", "pha", "trm", "psh", "lad",
                                "fld", "ovd", "cru", "lpg", "dyn", "awh", "res"};
    int n = 0, count = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    for (int i = 0; i < n; i++) {
        bool want = false;
        for (const char* id : pass) want |= strcmp(id, t[i]->id) == 0;
        TEST_ASSERT_EQUAL_MESSAGE(want, bypassPasses(*t[i]), t[i]->id);
        count += want;
    }
    TEST_ASSERT_EQUAL(17, count);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_basic_voice);
    RUN_TEST(test_bypass_passes_effect);
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
