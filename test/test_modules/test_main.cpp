#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "core/registry.h"
#include "core/engine.h"
#include "core/patch_io.h"
#include <cstdio>
#include "core/modules/osc.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"
#include "core/modules/env.h"
#include "core/modules/lfo.h"
#include "core/modules/keys.h"
#include "core/modules/out.h"
#include "core/modules/dly.h"
#include "core/modules/mix.h"
#include "core/modules/atn.h"
#include "core/modules/clk.h"
#include "core/modules/snh.h"
#include "core/modules/dmx.h"
#include "core/modules/mux.h"
#include "core/modules/selector.h"
#include "core/modules/tilt.h"
#include "core/modules/fx.h"
#include "core/modules/proc.h"
#include "core/modules/src.h"
#include "core/modules/mod.h"
#include "core/modules/grd.h"
#include "core/modules/kit.h"
#include "core/scales.h"
#include "core/delay_pool.h"
#include "core/dsp_util.h"
#include "core/modules/v14.h"

void setUp() {}
void tearDown() {}

// Runs nBlocks blocks; ins[i] is a constant-per-block pointer pattern repeated each block.
static std::vector<float> runBlocks(Module& m, const float* const* ins, const bool* connected,
                                    int nBlocks) {
    std::vector<float> r;
    for (int b = 0; b < nBlocks; b++) {
        m.process(ins, connected);
        r.insert(r.end(), m.out(0), m.out(0) + kBlock);
    }
    return r;
}

static float crossingHz(const std::vector<float>& s) {
    int n = 0;
    for (size_t i = 1; i < s.size(); i++)
        if (s[i - 1] < 0.f && s[i] >= 0.f) n++;
    return (float)n * kSampleRate / (float)s.size();
}

static void constBlock(float* b, float v) {
    for (int i = 0; i < kBlock; i++) b[i] = v;
}

void test_registry() {
    TEST_ASSERT_TRUE(findType("osc") == &kOscType);
    TEST_ASSERT_TRUE(findType("vca") == &kVcaType);
    TEST_ASSERT_NULL(findType("nope"));
    TEST_ASSERT_TRUE(findType("mix") == &kMixType);
    TEST_ASSERT_TRUE(findType("atn") == &kAtnType);
    TEST_ASSERT_TRUE(findType("snh") == &kSnhType);
    TEST_ASSERT_TRUE(findType("clk") == &kClkType);
    TEST_ASSERT_TRUE(findType("mux") == &kMuxType);
    TEST_ASSERT_TRUE(findType("dmx") == &kDmxType);
    TEST_ASSERT_TRUE(findType("tilt") == &kTiltType);
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    TEST_ASSERT_TRUE(n >= 2);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) TEST_ASSERT_TRUE(strcmp(t[i]->id, t[j]->id) != 0);
}

static float oscHz(float pitchV, float oct) {
    Module* m = kOscType.create();
    m->setParam(0, 0.f);  // sine
    m->setParam(1, oct);
    float pit[kBlock];
    constBlock(pit, pitchV);
    const float* ins[2] = {pit, nullptr};
    const bool conn[2] = {true, false};
    runBlocks(*m, ins, conn, 10);  // settle
    float hz = crossingHz(runBlocks(*m, ins, conn, 1500));
    delete m;
    return hz;
}

void test_osc_pitch() {
    TEST_ASSERT_FLOAT_WITHIN(261.63f * 0.005f, 261.63f, oscHz(0.f, 0.f));
    TEST_ASSERT_FLOAT_WITHIN(523.25f * 0.005f, 523.25f, oscHz(1.f, 0.f));
    TEST_ASSERT_FLOAT_WITHIN(130.81f * 0.005f, 130.81f, oscHz(0.f, -1.f));
}

void test_osc_amplitude() {
    Module* m = kOscType.create();  // default wave = saw
    const float* ins[2] = {nullptr, nullptr};
    const bool conn[2] = {false, false};
    auto s = runBlocks(*m, ins, conn, 200);
    float mx = -100.f, mn = 100.f;
    for (float v : s) { mx = fmaxf(mx, v); mn = fminf(mn, v); }
    TEST_ASSERT_TRUE(mx > 4.5f && mx < 5.5f);
    TEST_ASSERT_TRUE(mn < -4.5f && mn > -5.5f);
    TEST_ASSERT_FLOAT_WITHIN(1.f, 261.63f, m->monitor(0));
    delete m;
}

void test_vca_normalled() {
    Module* m = kVcaType.create();
    float in[kBlock], cv[kBlock];
    constBlock(in, 4.f);
    m->setParam(0, 0.5f);
    {
        const float* ins[2] = {in, nullptr};
        const bool conn[2] = {true, false};
        auto s = runBlocks(*m, ins, conn, 20);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.f, s.back());
    }
    m->setParam(0, 1.f);
    constBlock(cv, 0.f);
    const float* ins[2] = {in, cv};
    const bool conn[2] = {true, true};
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.f, runBlocks(*m, ins, conn, 20).back());
    constBlock(cv, 10.f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.f, runBlocks(*m, ins, conn, 20).back());
    delete m;
}

// ---- VCF ----
static float sineRms(Module& m, float hz, float amp, int nBlocks, const float* cutCv = nullptr) {
    static float in[kBlock];
    float ph = 0.f, sum = 0.f;
    int cnt = 0;
    for (int b = 0; b < nBlocks; b++) {
        for (int i = 0; i < kBlock; i++) {
            in[i] = amp * sinf(6.2831853f * ph);
            ph += hz / kSampleRate;
            if (ph >= 1.f) ph -= 1.f;
        }
        const float* ins[3] = {in, cutCv, nullptr};
        const bool conn[3] = {true, cutCv != nullptr, false};
        m.process(ins, conn);
        if (b >= nBlocks / 2)
            for (int i = 0; i < kBlock; i++) { sum += m.out(0)[i] * m.out(0)[i]; cnt++; }
    }
    return sqrtf(sum / (float)cnt);
}

static float vcfRatio(float mode, float hz) {
    Module* m = kVcfType.create();
    m->setParam(0, 500.f);
    m->setParam(1, 0.f);
    m->setParam(2, mode);
    float r = sineRms(*m, hz, 5.f, 300) / (5.f * 0.70710678f);
    delete m;
    return r;
}

void test_vcf_lowpass() {
    TEST_ASSERT_TRUE(vcfRatio(0.f, 5000.f) < 0.1f);
    TEST_ASSERT_TRUE(vcfRatio(0.f, 100.f) > 0.8f);
}

void test_vcf_hp() {
    TEST_ASSERT_TRUE(vcfRatio(1.f, 5000.f) > 0.8f);
    TEST_ASSERT_TRUE(vcfRatio(1.f, 100.f) < 0.1f);
}

void test_vcf_cut_cv() {
    Module* m = kVcfType.create();
    m->setParam(0, 500.f);
    float cv[kBlock];
    constBlock(cv, 1.f);
    sineRms(*m, 440.f, 1.f, 300, cv);  // let the 10 ms cutoff slew settle
    TEST_ASSERT_FLOAT_WITHIN(10.f, 1000.f, m->monitor(0));
    delete m;
}

// ---- ENV ----
static void setEnv(Module& m, float a, float d, float s, float r) {
    m.setParam(0, a); m.setParam(1, d); m.setParam(2, s); m.setParam(3, r);
}

void test_env_shape() {
    Module* m = kEnvType.create();
    setEnv(*m, 10.f, 10.f, 0.5f, 10.f);
    float g[kBlock];
    constBlock(g, 5.f);
    const float* ins[2] = {g, nullptr};
    const bool conn[2] = {true, false};
    float peak = 0.f, at80 = 0.f;
    const int blocks100 = 150;  // 100 ms
    for (int b = 0; b < blocks100; b++) {
        m->process(ins, conn);
        for (int i = 0; i < kBlock; i++) peak = fmaxf(peak, m->out(0)[i]);
        if (b == 124) at80 = m->out(0)[kBlock - 1];  // ~ 83 ms
    }
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 5.f, peak);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 2.5f, at80);
    TEST_ASSERT_EQUAL_FLOAT(3.f, m->monitor(0));
    constBlock(g, 0.f);
    float last = 9.f;
    for (int b = 0; b < 150; b++) { m->process(ins, conn); last = m->out(0)[kBlock - 1]; }
    TEST_ASSERT_TRUE(last < 0.05f);
    delete m;
}

void test_env_retrig() {
    Module* m = kEnvType.create();
    setEnv(*m, 20.f, 10.f, 0.3f, 10.f);
    float g[kBlock], rt[kBlock];
    constBlock(g, 5.f);
    constBlock(rt, 0.f);
    const float* ins[2] = {g, rt};
    const bool conn[2] = {true, true};
    for (int b = 0; b < 200; b++) m->process(ins, conn);
    TEST_ASSERT_TRUE(m->monitor(1) < 0.4f);
    constBlock(rt, 5.f);
    m->process(ins, conn);  // 0.67 ms: the retrig edge restarts the attack
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->monitor(0));
    m->process(ins, conn);
    TEST_ASSERT_TRUE(m->monitor(1) > 0.3f);
    delete m;
}

void test_env_retrig_gate_low() {
    Module* m = kEnvType.create();
    setEnv(*m, 5.f, 10.f, 0.5f, 10.f);
    float g[kBlock], rt[kBlock];
    constBlock(g, 0.f);
    constBlock(rt, 0.f);
    const float* ins[2] = {g, rt};
    const bool conn[2] = {true, true};
    m->process(ins, conn);
    constBlock(rt, 5.f);
    m->process(ins, conn);  // retrig edge with the gate low
    constBlock(rt, 0.f);
    for (int b = 0; b < 300; b++) m->process(ins, conn);
    TEST_ASSERT_TRUE(m->out(0)[kBlock - 1] < 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.f, m->monitor(0));
    delete m;
}

// ---- LFO ----
static int lfoRises(float rateV) {
    Module* m = kLfoType.create();
    m->setParam(0, 0.f);
    m->setParam(1, 2.f);
    float r[kBlock];
    constBlock(r, rateV);
    const float* ins[2] = {r, r};
    const bool conn[2] = {true, false};
    int n = 0;
    float prev = 0.f;
    for (int b = 0; b < 3000; b++) {  // 2 s
        m->process(ins, conn);
        for (int i = 0; i < kBlock; i++) {
            if (prev < 0.f && m->out(0)[i] >= 0.f) n++;
            prev = m->out(0)[i];
        }
    }
    delete m;
    return n;
}

void test_lfo_rate() {
    TEST_ASSERT_INT_WITHIN(1, 4, lfoRises(0.f));
    TEST_ASSERT_INT_WITHIN(1, 8, lfoRises(1.f));
}

// ---- KEYS ----
static void idle(Module& m, int blocks) {
    const float* ins[1] = {nullptr};
    const bool conn[1] = {false};
    for (int b = 0; b < blocks; b++) m.process(ins, conn);
}

void test_keys_pitch_gate() {
    KeysModule* k = static_cast<KeysModule*>(kKeysType.create());
    k->noteOn(72);
    idle(*k, 5);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.f, k->out(0)[kBlock - 1]);
    TEST_ASSERT_EQUAL_FLOAT(5.f, k->out(1)[kBlock - 1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.f, k->out(2)[kBlock - 1]);  // vel 0.8 * 5
    k->gateOff();
    idle(*k, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->out(1)[kBlock - 1]);
    delete k;
}

void test_keys_glide() {
    KeysModule* k = static_cast<KeysModule*>(kKeysType.create());
    k->noteOn(60);
    idle(*k, 5);
    k->setParam(1, 100.f);
    k->noteOn(72);
    idle(*k, 75);  // 50 ms
    TEST_ASSERT_TRUE(k->out(0)[kBlock - 1] < 0.9f);
    idle(*k, 900);  // 600 ms total
    TEST_ASSERT_TRUE(k->out(0)[kBlock - 1] > 0.99f);
    delete k;
}

void test_keys_retrigger_gap() {
    for (int legato = 0; legato < 2; legato++) {
        KeysModule* k = static_cast<KeysModule*>(kKeysType.create());
        k->setParam(3, (float)legato);
        k->noteOn(60);
        idle(*k, 1);
        k->noteOn(62);
        idle(*k, 1);
        int low = 0;
        for (int i = 0; i < kBlock; i++) low += k->out(1)[i] == 0.f;
        TEST_ASSERT_EQUAL_INT(legato ? 0 : kBlock, low);
        idle(*k, 1);
        for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(5.f, k->out(1)[i]);
        delete k;
    }
}

void test_keys_quick_tap() {
    KeysModule* k = static_cast<KeysModule*>(kKeysType.create());
    k->noteOn(64);
    k->gateOff();  // released before the audio thread saw it
    idle(*k, 1);
    for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(5.f, k->out(1)[i]);
    idle(*k, 1);
    for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(0.f, k->out(1)[i]);
    // a quick tap right after a held note still produces a gap, then one high block
    k->noteOn(60);
    idle(*k, 2);
    k->noteOn(62);
    k->gateOff();
    idle(*k, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->out(1)[0]);
    idle(*k, 1);
    TEST_ASSERT_EQUAL_FLOAT(5.f, k->out(1)[0]);
    idle(*k, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->out(1)[0]);
    delete k;
}

// ---- OUT ----
static float outPeak(OutModule& o, float amp, int blocks, float badValue = 0.f, bool useBad = false) {
    float in[kBlock], ph = 0.f, pk = 0.f;
    const float* ins[1] = {in};
    const bool conn[1] = {true};
    for (int b = 0; b < blocks; b++) {
        for (int i = 0; i < kBlock; i++) {
            in[i] = useBad ? badValue : amp * sinf(6.2831853f * ph);
            ph += 1000.f / kSampleRate;
            if (ph >= 1.f) ph -= 1.f;
        }
        o.process(ins, conn);
        if (b >= blocks - 10)
            for (int i = 0; i < kBlock; i++) {
                const float y = o.masterOut()[i];
                if (!(fabsf(y) <= 1.f)) return 99.f;  // also catches NaN
                pk = fmaxf(pk, fabsf(y));
            }
    }
    return pk;
}

void test_out_volume_select() {
    OutModule* o = static_cast<OutModule*>(kOutType.create());
    o->setParam(0, 0.5f);
    o->setParam(1, 1.f);
    o->setParam(2, 0.f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, softClip(0.5f), outPeak(*o, 5.f, 300));
    o->setParam(2, 1.f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, softClip(1.f), outPeak(*o, 5.f, 300));
    o->setParam(2, 0.f);
    o->setAutoDest(1);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, softClip(1.f), outPeak(*o, 5.f, 300));
    TEST_ASSERT_EQUAL_FLOAT(1.f, o->monitor(1));
    delete o;
}

void test_out_trim() {
    OutModule* o = static_cast<OutModule*>(kOutType.create());
    o->setParam(1, 0.5f);
    o->setParam(2, 1.f);  // LINE
    TEST_ASSERT_FLOAT_WITHIN(0.01f, softClip(0.1f), outPeak(*o, 1.f, 300));  // 1 V = 0.2 FS x 0.5
    o->setParam(3, 12.f);  // +12 dB: x 3.98
    TEST_ASSERT_FLOAT_WITHIN(0.01f, softClip(0.398f), outPeak(*o, 1.f, 300));
    delete o;
}

void test_out_bounded() {
    OutModule* o = static_cast<OutModule*>(kOutType.create());
    o->setParam(0, 1.f);
    TEST_ASSERT_TRUE(outPeak(*o, 0.f, 50, 1e6f, true) <= 1.f);
    TEST_ASSERT_TRUE(outPeak(*o, 0.f, 50, NAN, true) <= 1.f);
    delete o;
}

// ---- delay pool / DLY ----
void test_pool_exhaustion() {
    int16_t* s[kDelaySlots];
    for (int i = 0; i < kDelaySlots; i++) {
        s[i] = delayPoolAcquire();
        TEST_ASSERT_NOT_NULL(s[i]);
        for (int j = 0; j < i; j++) TEST_ASSERT_TRUE(s[i] != s[j]);
    }
    TEST_ASSERT_EQUAL_INT(0, delayPoolFree());
    TEST_ASSERT_NULL(delayPoolAcquire());
    delayPoolRelease(s[0]);
    int16_t* again = delayPoolAcquire();
    TEST_ASSERT_NOT_NULL(again);
    delayPoolRelease(again);
    for (int i = 1; i < kDelaySlots; i++) delayPoolRelease(s[i]);
    TEST_ASSERT_EQUAL_INT(kDelaySlots, delayPoolFree());
}

void test_dly_create_refused() {
    int16_t* s[kDelaySlots];
    for (int i = 0; i < kDelaySlots; i++) s[i] = delayPoolAcquire();
    TEST_ASSERT_NULL(kDlyType.create());
    for (int i = 0; i < kDelaySlots; i++) delayPoolRelease(s[i]);
}

void test_dly_echo() {
    Module* m = kDlyType.create();
    TEST_ASSERT_NOT_NULL(m);
    m->setParam(0, 100.f);
    m->setParam(1, 0.f);
    m->setParam(2, 1.f);
    float in[kBlock] = {};
    in[0] = 5.f;
    const float* ins[3] = {in, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    int peakAt = -1;
    float peak = 0.f;
    for (int b = 0; b < 200; b++) {  // 6400 samples
        m->process(ins, conn);
        in[0] = 0.f;
        for (int i = 0; i < kBlock; i++)
            if (fabsf(m->out(0)[i]) > peak) { peak = fabsf(m->out(0)[i]); peakAt = b * kBlock + i; }
    }
    TEST_ASSERT_INT_WITHIN(2, 4800, peakAt);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.f, peak);
    delete m;
    TEST_ASSERT_EQUAL_INT(kDelaySlots, delayPoolFree());
}

void test_dly_feedback_bounded() {
    Module* m = kDlyType.create();
    m->setParam(0, 50.f);
    m->setParam(1, 0.95f);
    m->setParam(2, 1.f);
    float in[kBlock], ph = 0.f;
    const float* ins[3] = {in, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    for (int b = 0; b < 15000; b++) {  // 10 s
        for (int i = 0; i < kBlock; i++) {
            in[i] = 10.f * sinf(6.2831853f * ph);
            ph += 220.f / kSampleRate;
            if (ph >= 1.f) ph -= 1.f;
        }
        m->process(ins, conn);
        for (int i = 0; i < kBlock; i++) TEST_ASSERT_TRUE(fabsf(m->out(0)[i]) <= 12.f);
    }
    delete m;
}

void test_dly_tail_decays_to_zero() {
    Module* m = kDlyType.create();
    m->setParam(0, 50.f);
    m->setParam(1, 0.95f);
    m->setParam(2, 1.f);
    float in[kBlock] = {};
    in[0] = 5.f;
    const float* ins[3] = {in, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    for (int b = 0; b < 30 * 48000 / kBlock; b++) {  // 30 s
        m->process(ins, conn);
        in[0] = 0.f;
    }
    // a flat 10 LSB fill (the worst case for rounding) must also decay
    for (int i = 0; i < kBlock; i++) in[i] = 10.f * 10.f / 32767.f;
    for (int b = 0; b < 200; b++) m->process(ins, conn);
    for (int i = 0; i < kBlock; i++) in[i] = 0.f;
    for (int b = 0; b < 30 * 48000 / kBlock; b++) m->process(ins, conn);
    for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(0.f, m->out(0)[i]);
    delete m;
}

void test_dly_mix_smoothed() {
    Module* m = kDlyType.create();
    m->setParam(2, 0.f);
    float in[kBlock];
    for (int i = 0; i < kBlock; i++) in[i] = 4.f;
    const float* ins[3] = {in, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    m->process(ins, conn);
    m->setParam(2, 1.f);  // wet is silent, so mix 0 -> 1 must fade the dry out, not cut
    m->process(ins, conn);
    TEST_ASSERT_TRUE(m->out(0)[0] > 3.9f);
    TEST_ASSERT_TRUE(m->out(0)[kBlock - 1] > 3.f);
    delete m;
}

// ---- v1.1 utilities ----
static float settle(Module& m, const float* const* ins, const bool* conn, int nBlocks = 40) {
    for (int b = 0; b < nBlocks; b++) m.process(ins, conn);
    return m.out(0)[kBlock - 1];
}

void test_mix_sums_levels() {
    Module* m = kMixType.create();
    float a[kBlock], c[kBlock];
    constBlock(a, 1.f);
    constBlock(c, 2.f);
    m->setParam(2, 0.5f);  // L3
    const float* ins[4] = {a, nullptr, c, nullptr};
    const bool conn[4] = {true, false, true, false};
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.f, settle(*m, ins, conn));
    delete m;
}

void test_mix_unconnected_silent() {
    Module* m = kMixType.create();
    const float* ins[4] = {nullptr, nullptr, nullptr, nullptr};
    const bool conn[4] = {false, false, false, false};
    TEST_ASSERT_EQUAL_FLOAT(0.f, settle(*m, ins, conn));
    delete m;
}

void test_atn_inverts_and_offsets() {
    Module* m = kAtnType.create();
    float a[kBlock];
    constBlock(a, 2.f);
    m->setParam(0, -0.5f);  // AMT
    m->setParam(1, 1.f);    // OFS
    const float* ins[1] = {a};
    const bool conn[1] = {true};
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.f, settle(*m, ins, conn));
    delete m;
}

void test_atn_no_input_is_dc() {
    Module* m = kAtnType.create();
    m->setParam(1, -3.f);
    const float* ins[1] = {nullptr};
    const bool conn[1] = {false};
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -3.f, settle(*m, ins, conn));
    delete m;
}

void test_snh_holds_on_trigger() {
    Module* m = kSnhType.create();
    float in[kBlock], trig[kBlock];
    const float* ins[3] = {in, trig, nullptr};
    const bool conn[3] = {true, true, false};
    for (int b = 0; b < 7; b++) {
        constBlock(in, (float)b);
        constBlock(trig, b >= 3 ? 5.f : 0.f);
        m->process(ins, conn);
        if (b >= 3) TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.f, m->out(0)[kBlock - 1]);
    }
    delete m;
}

void test_snh_random_without_input() {
    Module* m = kSnhType.create();
    float trig[kBlock];
    const float* ins[3] = {nullptr, trig, nullptr};
    const bool conn[3] = {false, true, false};
    float v[4];
    for (int k = 0; k < 4; k++) {
        constBlock(trig, 0.f);
        m->process(ins, conn);
        constBlock(trig, 5.f);
        m->process(ins, conn);
        v[k] = m->out(0)[kBlock - 1];
        TEST_ASSERT_TRUE(v[k] >= -5.f && v[k] <= 5.f);
    }
    TEST_ASSERT_FALSE(v[0] == v[1] && v[1] == v[2] && v[2] == v[3]);
    delete m;
}

void test_snh_slew() {
    Module* m = kSnhType.create();
    m->setParam(0, 100.f);  // SLW ms
    float in[kBlock], trig[kBlock];
    constBlock(in, 5.f);
    constBlock(trig, 5.f);
    const float* ins[3] = {in, trig, nullptr};
    const bool conn[3] = {true, true, false};
    m->process(ins, conn);
    m->process(ins, conn);  // ~1.3 ms after the hold
    const float y = m->out(0)[kBlock - 1];
    TEST_ASSERT_TRUE(y > 0.f && y < 5.f);
    delete m;
}

void test_snh_holds_without_trigger() {
    Module* m = kSnhType.create();
    float in[kBlock];
    constBlock(in, 4.f);
    const float* ins[3] = {in, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    TEST_ASSERT_EQUAL_FLOAT(0.f, settle(*m, ins, conn));
    delete m;
}

// Sample indices where out(o) rises above 2.5 V.
static std::vector<long> rises(Module& m, const float* const* ins, const bool* conn, int o, int nBlocks) {
    std::vector<long> r;
    float prev = 0.f;
    for (int b = 0; b < nBlocks; b++) {
        m.process(ins, conn);
        for (int i = 0; i < kBlock; i++) {
            const float v = m.out(o)[i];
            if (prev < 2.5f && v >= 2.5f) r.push_back((long)b * kBlock + i);
            prev = v;
        }
    }
    return r;
}

void test_clk_120bpm_period() {
    Module* m = kClkType.create();
    const float* ins[3] = {nullptr, nullptr, nullptr};
    const bool conn[3] = {false, false, false};
    const std::vector<long> r = rises(*m, ins, conn, 0, 24000 * 4 / kBlock);
    TEST_ASSERT_TRUE(r.size() >= 4);
    TEST_ASSERT_EQUAL(0, r[0]);  // starts high
    for (size_t i = 1; i < r.size(); i++) TEST_ASSERT_INT_WITHIN(1, 24000, r[i] - r[i - 1]);
    delete m;
}

void test_clk_div3() {
    Module* m = kClkType.create();
    m->setParam(0, 300.f);  // BPM: 9600 samples per beat
    m->setParam(1, 2.f);    // DIV /3
    const float* ins[3] = {nullptr, nullptr, nullptr};
    const bool conn[3] = {false, false, false};
    Module* ref = kClkType.create();
    ref->setParam(0, 300.f);
    const std::vector<long> base = rises(*ref, ins, conn, 0, 9600 * 9 / kBlock);
    const std::vector<long> div = rises(*m, ins, conn, 1, 9600 * 9 / kBlock);
    TEST_ASSERT_EQUAL(9, (int)base.size());
    TEST_ASSERT_EQUAL(3, (int)div.size());
    for (size_t i = 0; i < div.size(); i++) TEST_ASSERT_EQUAL(base[i * 3], div[i]);
    delete m;
    delete ref;
}

void test_clk_external_clock() {
    Module* m = kClkType.create();
    float clk[kBlock];
    const float* ins[3] = {clk, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    for (int b = 0; b < 12; b++) {
        const bool high = (b / 2) % 2 == 1;
        constBlock(clk, high ? 5.f : 0.f);
        m->process(ins, conn);
        for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(high ? 5.f : 0.f, m->out(0)[i]);
    }
    delete m;
}

void test_clk_reset() {
    Module* m = kClkType.create();  // 120 BPM: low from sample 12000 to 24000
    float rst[kBlock];
    const float* ins[3] = {nullptr, rst, nullptr};
    const bool conn[3] = {false, true, false};
    constBlock(rst, 0.f);
    for (int b = 0; b < 16000 / kBlock; b++) m->process(ins, conn);
    TEST_ASSERT_EQUAL_FLOAT(0.f, m->out(0)[kBlock - 1]);
    constBlock(rst, 5.f);
    m->process(ins, conn);
    TEST_ASSERT_EQUAL_FLOAT(5.f, m->out(0)[0]);
    TEST_ASSERT_EQUAL_FLOAT(5.f, m->out(1)[0]);  // the divider restarts too
    delete m;
}

void test_selector_cv() {
    Selector sel;
    float v[kBlock];
    const float cases[][3] = {{0, 0.f, 0}, {0, 2.f, 1}, {0, 4.f, 2}, {0, 6.f, 2}, {0, -1.f, 0}, {2, 2.f, 0}};
    for (const auto& c : cases) {
        constBlock(v, c[1]);
        TEST_ASSERT_EQUAL((int)c[2], sel.index((int)c[0], false, v, 0));
    }
    TEST_ASSERT_EQUAL(1, sel.index(1, false, nullptr, 0));  // unpatched SEL: POS alone
}

void test_selector_step() {
    Selector sel;
    float lo[kBlock], hi[kBlock];
    constBlock(lo, 0.f);
    constBlock(hi, 5.f);
    TEST_ASSERT_EQUAL(0, sel.index(0, true, lo, 0));
    const int expect[] = {1, 2, 0};
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_EQUAL(expect[k], sel.index(0, true, hi, 0));
        TEST_ASSERT_EQUAL(expect[k], sel.index(0, true, hi, 1));  // still high: no advance
        sel.index(0, true, lo, 0);
    }
}

void test_mux_picks_input() {
    Module* m = kMuxType.create();
    float a[kBlock], b[kBlock], c[kBlock];
    constBlock(a, 1.f);
    constBlock(b, 2.f);
    constBlock(c, 3.f);
    m->setParam(0, 1.f);  // POS B
    const float* ins[4] = {a, b, c, nullptr};
    const bool conn[4] = {true, true, true, false};
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.f, settle(*m, ins, conn));
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->monitor(0));
    delete m;
}

void test_mux_switch_is_smooth() {
    Module* m = kMuxType.create();
    float a[kBlock], b[kBlock];
    constBlock(a, 0.f);
    constBlock(b, 1.f);
    const float* ins[4] = {a, b, nullptr, nullptr};
    const bool conn[4] = {true, true, false, false};
    settle(*m, ins, conn);
    m->setParam(0, 1.f);
    float prev = m->out(0)[kBlock - 1];
    for (int blk = 0; blk < 20; blk++) {
        m->process(ins, conn);
        for (int i = 0; i < kBlock; i++) {
            TEST_ASSERT_TRUE(fabsf(m->out(0)[i] - prev) <= 0.2f);
            prev = m->out(0)[i];
        }
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.f, settle(*m, ins, conn));
    delete m;
}

void test_dmx_routes() {
    Module* m = kDmxType.create();
    float in[kBlock];
    constBlock(in, 1.f);
    m->setParam(0, 2.f);  // POS C
    const float* ins[3] = {in, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    for (int b = 0; b < 40; b++) m->process(ins, conn);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.f, m->out(0)[kBlock - 1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.f, m->out(1)[kBlock - 1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.f, m->out(2)[kBlock - 1]);
    delete m;
}

void test_tilt_module_scales() {
    Module* m = kTiltType.create();
    m->setParam(1, 20.f);  // SMO minimum
    tiltPublish(22.5f, -45.f);
    const float* ins[1] = {nullptr};
    const bool conn[1] = {false};
    for (int b = 0; b < 600; b++) m->process(ins, conn);
    // a 20 ms one-pole stalls ~1e-4 short: its step falls below a float ulp of the target
    TEST_ASSERT_FLOAT_WITHIN(5e-4f, 2.5f, m->out(0)[kBlock - 1]);
    TEST_ASSERT_FLOAT_WITHIN(5e-4f, -5.f, m->out(1)[kBlock - 1]);
    delete m;
}

void test_tilt_module_clamps() {
    Module* m = kTiltType.create();
    m->setParam(1, 20.f);
    tiltPublish(90.f, 0.f);
    const float* ins[1] = {nullptr};
    const bool conn[1] = {false};
    for (int b = 0; b < 600; b++) m->process(ins, conn);
    TEST_ASSERT_FLOAT_WITHIN(5e-4f, 5.f, m->out(0)[kBlock - 1]);
    delete m;
}

void test_tilt_module_smooths() {
    Module* m = kTiltType.create();
    m->setParam(1, 100.f);
    tiltPublish(0.f, 0.f);
    const float* ins[1] = {nullptr};
    const bool conn[1] = {false};
    for (int b = 0; b < 10; b++) m->process(ins, conn);
    tiltPublish(45.f, 0.f);
    m->process(ins, conn);
    const float y = m->out(0)[kBlock - 1];
    TEST_ASSERT_TRUE(y > 0.f && y < 5.f);
    tiltPublish(0.f, 0.f);
    delete m;
}

void test_clk_reset_while_high_retriggers() {
    Module* m = kClkType.create();  // 120 BPM: high for samples 0..11999
    float rst[kBlock];
    const float* ins[3] = {nullptr, rst, nullptr};
    const bool conn[3] = {false, true, false};
    constBlock(rst, 0.f);
    for (int b = 0; b < 6000 / kBlock; b++) m->process(ins, conn);
    TEST_ASSERT_EQUAL_FLOAT(5.f, m->out(0)[kBlock - 1]);
    constBlock(rst, 5.f);
    bool sawLow = false, highAfterLow = false;
    for (int b = 0; b < 4; b++) {
        m->process(ins, conn);
        for (int i = 0; i < kBlock; i++) {
            if (m->out(0)[i] == 0.f) sawLow = true;
            else if (sawLow) highAfterLow = true;
        }
    }
    TEST_ASSERT_TRUE(sawLow);
    TEST_ASSERT_TRUE(highAfterLow);
    delete m;
}

void test_selector_cv_hysteresis() {
    Selector sel;
    float v[kBlock];
    constBlock(v, 1.75f);  // above the 1.667 V boundary by more than the 50 mV band
    TEST_ASSERT_EQUAL(1, sel.index(0, false, v, 0));
    constBlock(v, 1.64f);  // just below, inside the band: stays
    TEST_ASSERT_EQUAL(1, sel.index(0, false, v, 0));
    constBlock(v, 1.55f);  // clearly below: switches
    TEST_ASSERT_EQUAL(0, sel.index(0, false, v, 0));
    constBlock(v, 1.69f);  // inside the band from below: stays
    TEST_ASSERT_EQUAL(0, sel.index(0, false, v, 0));
}

void test_tilt_smooth_minimum() {
    TEST_ASSERT_TRUE(kTiltType.params[1].min >= 20.f);  // the IMU is read at 50 Hz
}


// ---- v1.2: every registered type ----
// Feeds silence, then +-5 V noise on every input with a gate on the even inputs, for 200 blocks;
// every output must stay finite and within +-25 V (the engine clamps at 50).
void test_all_types_sane() {
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    static float bufs[kMaxIns][kBlock];
    const float* ins[kMaxIns];
    bool conn[kMaxIns];
    uint32_t rng = 12345;
    for (int t = 0; t < n; t++) {
        Module* m = types[t]->create();
        TEST_ASSERT_NOT_NULL_MESSAGE(m, types[t]->id);
        for (int p = 0; p < types[t]->numParams; p++) m->setParam(p, types[t]->params[p].def);
        for (int i = 0; i < kMaxIns; i++) {
            ins[i] = bufs[i];
            conn[i] = false;
        }
        for (int b = 0; b < 400; b++) {
            const bool live = b >= 100;
            for (int i = 0; i < kMaxIns; i++) {
                conn[i] = live && i < types[t]->numIns;
                for (int k = 0; k < kBlock; k++) {
                    rng = rng * 1664525u + 1013904223u;
                    const float noise = ((float)(rng >> 8) / 16777216.f * 2.f - 1.f) * 5.f;
                    bufs[i][k] = (i % 2 == 0) ? (((b / 8) % 2) ? 5.f : 0.f) : noise;
                }
            }
            m->process(ins, conn);
            for (int o = 0; o < types[t]->numOuts; o++)
                for (int k = 0; k < kBlock; k++) {
                    const float v = m->out(o)[k];
                    if (!std::isfinite(v) || fabsf(v) > 25.f) {
                        char msg[64];
                        snprintf(msg, sizeof msg, "%s out %d block %d = %f", types[t]->id, o, b, (double)v);
                        TEST_FAIL_MESSAGE(msg);
                    }
                }
            if (const float* mo = m->masterOut())
                for (int k = 0; k < kBlock; k++) TEST_ASSERT_TRUE_MESSAGE(std::isfinite(mo[k]), types[t]->id);
        }
        delete m;
    }
}

void test_all_types_round_trip() {
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    for (int t = 0; t < n; t++) {
        Patch p;
        uint16_t id = 0;
        TEST_ASSERT_EQUAL(PatchError::None, p.addModule(*types[t], 0, 0, &id));
        ModuleRec* r = p.find(id);
        for (int k = 0; k < types[t]->numParams; k++) r->params[k] = paramCoarse(types[t]->params[k], 3);
        Patch q;
        TEST_ASSERT_EQUAL_MESSAGE((int)LoadError::None, (int)patchFromJson(patchToJson(p), q, nullptr).error, types[t]->id);
        const ModuleRec* back = q.find(id);
        TEST_ASSERT_NOT_NULL_MESSAGE(back, types[t]->id);
        TEST_ASSERT_TRUE_MESSAGE(back->type == types[t], types[t]->id);
        for (int k = 0; k < types[t]->numParams; k++)
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1e-4f * (1.f + fabsf(r->params[k])), r->params[k], back->params[k], types[t]->id);
    }
}

void test_all_types_unique_keys() {
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE_MESSAGE(t[i]->numParams <= kMaxParams, t[i]->id);
        TEST_ASSERT_TRUE_MESSAGE(t[i]->numIns <= kMaxIns, t[i]->id);
        TEST_ASSERT_TRUE_MESSAGE(t[i]->numOuts <= kMaxOuts, t[i]->id);
        for (int a = 0; a < t[i]->numParams; a++) {
            TEST_ASSERT_TRUE_MESSAGE(strlen(t[i]->params[a].label) <= 3, t[i]->params[a].key);
            for (int b = a + 1; b < t[i]->numParams; b++)
                TEST_ASSERT_TRUE_MESSAGE(strcmp(t[i]->params[a].key, t[i]->params[b].key) != 0, t[i]->id);
        }
        for (int a = 0; a < t[i]->numIns; a++) TEST_ASSERT_TRUE_MESSAGE(strlen(t[i]->inLabels[a]) <= 3, t[i]->id);
        for (int a = 0; a < t[i]->numOuts; a++) TEST_ASSERT_TRUE_MESSAGE(strlen(t[i]->outLabels[a]) <= 3, t[i]->id);
    }
}


// ---- v1.2 Task 8: effects ----
static float rms(const std::vector<float>& v, size_t from = 0) {
    double s = 0;
    for (size_t i = from; i < v.size(); i++) s += (double)v[i] * v[i];
    return (float)std::sqrt(s / (double)(v.size() - from));
}

void test_rev_tail_decays() {
    Module* m = kRevType.create();
    TEST_ASSERT_NOT_NULL(m);
    static float b[kBlock];
    const float* ins[1] = {b};
    bool c[1] = {true};
    for (int k = 0; k < kBlock; k++) b[k] = (k % 8 < 4) ? 5.f : -5.f;
    for (int i = 0; i < 50; i++) m->process(ins, c);  // a burst
    constBlock(b, 0.f);
    std::vector<float> wet;
    for (int i = 0; i < 1500; i++) {  // 1 s of tail
        m->process(ins, c);
        wet.insert(wet.end(), m->out(1), m->out(1) + kBlock);
    }
    const float early = rms(std::vector<float>(wet.begin(), wet.begin() + 4800));
    const float late = rms(std::vector<float>(wet.end() - 4800, wet.end()));
    TEST_ASSERT_TRUE(early > 0.05f);  // there is a tail
    TEST_ASSERT_TRUE(late < early * 0.5f);  // and it decays
    delete m;
}

void test_pool_shared_dly_rev() {
    Engine eng([]() -> uint32_t { return 1000000; });
    Patch p;
    TEST_ASSERT_TRUE(eng.canAdd(p, kDlyType).ok);
    p.addModule(kDlyType, 0, 0, nullptr);
    TEST_ASSERT_TRUE(eng.canAdd(p, kRevType).ok);
    p.addModule(kRevType, 0, 1, nullptr);
    const Refusal r = eng.canAdd(p, kRevType);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_STRING("NO BUFFER", r.reason);
    TEST_ASSERT_FALSE(eng.canAdd(p, kDlyType).ok);
}

void test_psh_octave_up() {
    Module* m = kPshType.create();
    m->setParam(0, 12.f);
    m->setParam(2, 1.f);
    static float b[kBlock];
    const float* ins[2] = {b, b};
    bool c[2] = {true, false};
    float ph = 0.f;
    std::vector<float> o;
    for (int blk = 0; blk < 600; blk++) {
        for (int k = 0; k < kBlock; k++) {
            b[k] = 5.f * sinf(ph);
            ph += 2.f * 3.14159265f * 220.f / kSampleRate;
        }
        m->process(ins, c);
        if (blk > 100) o.insert(o.end(), m->out(0), m->out(0) + kBlock);
    }
    TEST_ASSERT_FLOAT_WITHIN(40.f, 440.f, crossingHz(o));
    delete m;
}


// ---- v1.2 Task 9: processors ----
// Runs `t` with a sine of `hz` / `volts` on input 0 (and `in1` held on input 1), returns out `o`.
static std::vector<float> runSine(const ModuleTypeInfo& t, float hz, float volts, int blocks, int o = 0,
                                  float in1 = 0.f, bool in1On = false, const float* params = nullptr) {
    Module* m = t.create();
    if (params)
        for (int i = 0; i < t.numParams; i++) m->setParam(i, params[i]);
    static float b0[kBlock], b1[kBlock];
    const float* ins[kMaxIns] = {b0, b1, b1, b1, b1, b1, b1, b1};
    bool c[kMaxIns] = {true, in1On};
    std::vector<float> r;
    float ph = 0.f;
    for (int blk = 0; blk < blocks; blk++) {
        for (int k = 0; k < kBlock; k++) {
            b0[k] = volts * sinf(ph);
            ph += 6.2831853f * hz / kSampleRate;
            b1[k] = in1;
        }
        m->process(ins, c);
        r.insert(r.end(), m->out(o), m->out(o) + kBlock);
    }
    delete m;
    return r;
}

void test_lad_lowpasses() {
    const float lo = rms(runSine(kLadType, 100.f, 2.f, 300), 4800);
    const float hi = rms(runSine(kLadType, 8000.f, 2.f, 300), 4800);
    TEST_ASSERT_TRUE(lo > 0.5f);
    TEST_ASSERT_TRUE(hi < lo * 0.1f);
}

void test_vcf_bp_mode() {
    const float p[] = {1000.f, 0.2f, 2.f, 0.f};
    const float mid = rms(runSine(kVcfType, 1000.f, 2.f, 300, 0, 0.f, false, p), 4800);
    const float low = rms(runSine(kVcfType, 50.f, 2.f, 300, 0, 0.f, false, p), 4800);
    TEST_ASSERT_TRUE(mid > low * 4.f);
}

void test_fld_folds_above_threshold() {
    const float p[] = {6.f, 0.f};
    std::vector<float> y = runSine(kFldType, 100.f, 5.f, 100, 0, 0.f, false, p);
    float mx = 0.f;
    for (float v : y) mx = fabsf(v) > mx ? fabsf(v) : mx;
    TEST_ASSERT_TRUE(mx <= 5.01f);
    // folding adds zero crossings: many more than the input's 2 per cycle
    TEST_ASSERT_TRUE(crossingHz(y) > 300.f);
}

void test_cru_quantizes() {
    const float p[] = {2.f, 48000.f, 1.f};
    std::vector<float> y = runSine(kCruType, 100.f, 5.f, 50, 0, 0.f, false, p);
    for (float v : y) {
        const float q = v / 2.5f;  // 2 bits: steps of 2.5 V
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, roundf(q), q);
    }
}

void test_lpg_closes_after_decay() {
    Module* m = kLpgType.create();
    m->setParam(0, 50.f);
    static float b0[kBlock], tr[kBlock];
    const float* ins[3] = {b0, b0, tr};
    bool c[3] = {true, false, true};
    for (int k = 0; k < kBlock; k++) b0[k] = (k % 16 < 8) ? 4.f : -4.f;
    constBlock(tr, 5.f);
    m->process(ins, c);
    constBlock(tr, 0.f);
    for (int i = 0; i < 4; i++) m->process(ins, c);
    float early = 0.f;
    for (int k = 0; k < kBlock; k++) early += fabsf(m->out(0)[k]);
    for (int i = 0; i < 600; i++) m->process(ins, c);
    float late = 0.f;
    for (int k = 0; k < kBlock; k++) late += fabsf(m->out(0)[k]);
    TEST_ASSERT_TRUE(early > 10.f);
    TEST_ASSERT_TRUE(late < early * 0.01f);
    delete m;
}

void test_ring_multiplies() {
    Module* m = kRingType.create();
    static float a[kBlock], b[kBlock];
    constBlock(a, 2.f);
    constBlock(b, -2.5f);
    const float* ins[2] = {a, b};
    bool c[2] = {true, true};
    m->process(ins, c);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.f, m->out(0)[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, m->out(1)[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.f, m->out(2)[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -2.5f, m->out(3)[0]);
    delete m;
}

void test_xfd_ends() {
    const float p0[] = {0.f}, p1[] = {1.f};
    Module* m = kXfdType.create();
    static float a[kBlock], b[kBlock];
    constBlock(a, 1.f);
    constBlock(b, 3.f);
    const float* ins[3] = {a, b, b};
    bool c[3] = {true, true, false};
    m->setParam(0, p0[0]);
    for (int i = 0; i < 50; i++) m->process(ins, c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.f, m->out(0)[kBlock - 1]);
    m->setParam(0, p1[0]);
    for (int i = 0; i < 50; i++) m->process(ins, c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 3.f, m->out(0)[kBlock - 1]);
    delete m;
}

void test_dyn_reduces_loud() {
    const float p[] = {-20.f, 10.f, 1.f, 100.f, 0.f};
    const float loud = rms(runSine(kDynType, 200.f, 5.f, 300, 0, 0.f, false, p), 4800);
    const float quiet = rms(runSine(kDynType, 200.f, 0.2f, 300, 0, 0.f, false, p), 4800);
    TEST_ASSERT_TRUE(loud / quiet < 25.f * 0.5f);  // input ratio is 25
    TEST_ASSERT_TRUE(rms(runSine(kDynType, 200.f, 5.f, 300, 1, 0.f, false, p), 4800) > 0.5f);  // GR out
}

void test_enf_tracks_level() {
    std::vector<float> e = runSine(kEnfType, 200.f, 4.f, 200);
    TEST_ASSERT_TRUE(e.back() > 2.f && e.back() < 4.5f);
    std::vector<float> g = runSine(kEnfType, 200.f, 4.f, 200, 1);
    TEST_ASSERT_EQUAL_FLOAT(5.f, g.back());
}

void test_slw_rise_time() {
    Module* m = kSlwType.create();
    m->setParam(0, 100.f);  // 10 V per 100 ms
    static float a[kBlock];
    constBlock(a, 5.f);
    const float* ins[1] = {a};
    bool c[1] = {true};
    const int half = (int)(0.05f * kSampleRate / kBlock);  // 50 ms -> 5 V
    for (int i = 0; i < half / 2; i++) m->process(ins, c);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 2.5f, m->out(0)[kBlock - 1]);
    for (int i = 0; i < half; i++) m->process(ins, c);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 5.f, m->out(0)[kBlock - 1]);
    delete m;
}


// ---- v1.2 Task 10: sources ----
// Runs `t` for `blocks` with nothing patched except a 5 V trigger on input `trigIn` in block 1.
static std::vector<float> runTrig(const ModuleTypeInfo& t, int blocks, int trigIn = 0, const float* params = nullptr) {
    Module* m = t.create();
    if (params)
        for (int i = 0; i < t.numParams; i++) m->setParam(i, params[i]);
    static float tr[kBlock], z[kBlock];
    const float* ins[kMaxIns] = {z, z, z, z, z, z, z, z};
    bool c[kMaxIns] = {};
    if (trigIn >= 0) {
        ins[trigIn] = tr;
        c[trigIn] = true;
    }
    std::vector<float> r;
    for (int b = 0; b < blocks; b++) {
        constBlock(tr, b == 1 ? 5.f : 0.f);
        m->process(ins, c);
        r.insert(r.end(), m->out(0), m->out(0) + kBlock);
    }
    delete m;
    return r;
}

void test_noi_kinds_differ() {
    float prev = -1.f;
    for (int k = 0; k < 6; k++) {
        const float p[] = {(float)k, 1000.f};
        std::vector<float> v = runTrig(kNoiType, 100, 0, p);
        const float r = rms(v);
        TEST_ASSERT_TRUE(r > 0.01f);
        TEST_ASSERT_TRUE(r != prev);
        prev = r;
    }
}

void test_kck_trig_makes_sound() {
    std::vector<float> v = runTrig(kKckType, 300);
    char msg[96];
    auto pk = [](const std::vector<float>& x) { float m = 0.f; for (float y : x) m = fabsf(y) > m ? fabsf(y) : m; return m; };
    snprintf(msg, sizeof msg, "kick rms %f pk %f snr pk %f hat pk %f str pk %f mdl pk %f", (double)rms(std::vector<float>(v.begin(), v.begin() + 2400)),
             (double)pk(v), (double)pk(runTrig(kSnrType, 100)), (double)pk(runTrig(kHatType, 100)), (double)pk(runTrig(kStrType, 100)), (double)pk(runTrig(kMdlType, 100)));
    TEST_ASSERT_TRUE_MESSAGE(rms(std::vector<float>(v.begin(), v.begin() + 2400)) > 0.3f, msg);
    TEST_ASSERT_TRUE_MESSAGE(pk(v) > 3.f && pk(runTrig(kHatType, 100)) > 3.f && pk(runTrig(kMdlType, 100)) < 7.f, msg);
    TEST_ASSERT_TRUE(rms(std::vector<float>(v.end() - 480, v.end())) < 0.05f);
    TEST_ASSERT_TRUE(rms(runTrig(kSnrType, 100)) > 0.05f);
    TEST_ASSERT_TRUE(rms(runTrig(kHatType, 100)) > 0.05f);
}

void test_str_pluck_decays() {
    std::vector<float> v = runTrig(kStrType, 1500);
    const float early = rms(std::vector<float>(v.begin(), v.begin() + 4800));
    const float late = rms(std::vector<float>(v.end() - 4800, v.end()));
    TEST_ASSERT_TRUE(early > 0.1f);
    TEST_ASSERT_TRUE(late < early * 0.3f);
    TEST_ASSERT_TRUE(rms(runTrig(kMdlType, 300)) > 0.02f);
}

void test_ssw_pitch() {
    const float p[] = {0.f, 0.f, 0.f, 0.f};  // no detune, no side saws: one saw at C4
    std::vector<float> v = runTrig(kSswType, 300, -1, p);
    TEST_ASSERT_FLOAT_WITHIN(261.63f * 0.02f, 261.63f, crossingHz(v));
}

void test_vso_pitch() {  // review r2: VSO was stuck at 220 Hz with SYNC off
    const float c4[] = {0.f, 0.f, 0.f, 0.5f, 0.f, 2.f};
    TEST_ASSERT_FLOAT_WITHIN(261.63f * 0.02f, 261.63f, crossingHz(runTrig(kVsoType, 300, -1, c4)));
    const float c5[] = {1.f, 0.f, 0.f, 0.5f, 0.f, 2.f};
    TEST_ASSERT_FLOAT_WITHIN(523.25f * 0.02f, 523.25f, crossingHz(runTrig(kVsoType, 300, -1, c5)));
    // SYNC: the reset rate (master) is the note, so the period stays C4's.
    const float sy[] = {0.f, 0.f, 0.f, 0.5f, 1.f, 1.5f};
    std::vector<float> v = runTrig(kVsoType, 300, -1, sy);
    const int per = (int)lroundf(kSampleRate / 261.63f);
    double diff = 0, ref = 0;
    for (size_t i = 4000; i + per < v.size(); i++) {
        diff += fabs(v[i] - v[i + per]);
        ref += fabs(v[i]);
    }
    TEST_ASSERT_TRUE(diff < 0.1 * ref);
}

void test_fmo_index_zero_is_sine() {
    const float p[] = {0.f, 0.f, 2.f, 0.f};
    std::vector<float> v = runTrig(kFmoType, 300, -1, p);
    TEST_ASSERT_FLOAT_WITHIN(261.63f * 0.02f, 261.63f, crossingHz(v));
    float mx = 0.f;
    for (float x : v) mx = fabsf(x) > mx ? fabsf(x) : mx;
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 5.f, mx);
}


// ---- v1.2 Task 11: modulators and sequencers ----
// Clocks module `m` (clock on input 0) `n` times; each clock is 4 blocks high, 4 low. Returns
// out `o` sampled at the end of each high phase.
static std::vector<float> clocked(Module* m, int n, int o) {
    static float ck[kBlock], z[kBlock];
    const float* ins[kMaxIns] = {ck, z, z, z, z, z, z, z};
    bool c[kMaxIns] = {true};
    std::vector<float> r;
    for (int k = 0; k < n; k++) {
        constBlock(ck, 5.f);
        for (int b = 0; b < 4; b++) m->process(ins, c);
        r.push_back(m->out(o)[kBlock - 1]);
        constBlock(ck, 0.f);
        for (int b = 0; b < 4; b++) m->process(ins, c);
    }
    return r;
}

void test_quantize_major() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.f, quantize(0.02f, 1, 0));            // C
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.f / 12.f, quantize(1.4f / 12.f, 1, 0));  // C#+ -> D
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 11.f / 12.f, quantize(10.6f / 12.f, 1, 0)); // A#+ -> B
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.f / 12.f, quantize(-0.9f / 12.f, 1, 0)); // below C -> B below
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 7.f / 12.f, quantize(6.6f / 12.f, 9, 0));  // maj pentatonic: F#+ -> G
    TEST_ASSERT_EQUAL(7, scaleSize(1));
    TEST_ASSERT_EQUAL(14, scaleDegree(1, 8));  // degree 8 of major = D an octave up
    TEST_ASSERT_EQUAL(-1, scaleDegree(1, -1));  // degree -1 = B below
}

void test_seq_steps_and_gates() {
    Module* m = kSeqType.create();
    for (int i = 0; i < 12; i++) m->setParam(i, (float)i);  // step i = i semitones
    m->setParam(13, 4.f);                                    // length 4
    m->setParam(14, 0.9f);  // gate 90% of the clock period (the test clock is high for 50%)
    std::vector<float> p = clocked(m, 9, 0);
    for (int k = 0; k < 9; k++) TEST_ASSERT_FLOAT_WITHIN(1e-5f, (float)(k % 4) / 12.f, p[k]);
    m->setParam(12, (float)0b1011);  // step 3 (index 2) gate off
    std::vector<float> g = clocked(m, 4, 1);  // any 4 consecutive steps hold the one gated-off step
    int offs = 0;
    for (float v : g) offs += v < 1.f;
    TEST_ASSERT_EQUAL(1, offs);
    delete m;
}

void test_trg_lanes() {
    Module* m = kTrgType.create();
    m->setParam(0, (float)0b0101);
    m->setParam(1, (float)0b0010);
    m->setParam(2, 0.f);
    m->setParam(3, 4.f);
    std::vector<float> a = clocked(m, 8, 0);
    const float expA[] = {5, 0, 5, 0, 5, 0, 5, 0};
    for (int k = 0; k < 8; k++) TEST_ASSERT_EQUAL_FLOAT(expA[k], a[k]);
    delete m;
    Module* n = kTrgType.create();
    n->setParam(1, (float)0b0010);
    n->setParam(3, 4.f);
    std::vector<float> b = clocked(n, 4, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.f, b[0]);
    TEST_ASSERT_EQUAL_FLOAT(5.f, b[1]);
    delete n;
}

void test_euc_e38() {
    const char* want = "x..x..x.";
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL(want[i] == 'x', euclidHit(8, 3, 0, i));
    Module* m = kEucType.create();
    std::vector<float> g = clocked(m, 8, 0);
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_FLOAT(want[i] == 'x' ? 5.f : 0.f, g[i]);
    delete m;
}

void test_tur_locked_loops() {
    Module* m = kTurType.create();
    m->setParam(0, 5.f);
    m->setParam(1, 0.f);  // never flips: a fixed 5-step loop
    std::vector<float> v = clocked(m, 20, 0);
    for (int k = 5; k < 20; k++) TEST_ASSERT_EQUAL_FLOAT(v[k - 5], v[k]);
    delete m;
}

void test_lgc_xor() {
    Module* m = kLgcType.create();
    static float a[kBlock], b[kBlock];
    const float* ins[2] = {a, b};
    bool c[2] = {true, true};
    const float va[] = {0, 5, 0, 5}, vb[] = {0, 0, 5, 5}, xr[] = {0, 5, 5, 0}, an[] = {0, 0, 0, 5};
    for (int k = 0; k < 4; k++) {
        constBlock(a, va[k]);
        constBlock(b, vb[k]);
        m->process(ins, c);
        TEST_ASSERT_EQUAL_FLOAT(xr[k], m->out(2)[0]);
        TEST_ASSERT_EQUAL_FLOAT(an[k], m->out(0)[0]);
    }
    delete m;
}

void test_lfo_old_waves_same() {
    TEST_ASSERT_EQUAL_STRING("SIN", kLfoType.params[0].enumNames[0]);
    TEST_ASSERT_EQUAL_STRING("SQR", kLfoType.params[0].enumNames[1]);
    Module* m = kLfoType.create();
    m->setParam(0, 1.f);
    m->setParam(1, 10.f);
    static float z[kBlock];
    const float* ins[2] = {z, z};
    bool c[2] = {false, false};
    m->process(ins, c);
    TEST_ASSERT_EQUAL_FLOAT(5.f, m->out(0)[0]);  // square starts high
    delete m;
}

void test_ad_loop() {
    Module* m = kAdType.create();
    m->setParam(0, 1.f);
    m->setParam(1, 5.f);
    m->setParam(3, 1.f);  // loop: cycles without a trigger
    static float z[kBlock];
    const float* ins[1] = {z};
    bool c[1] = {false};
    int eocs = 0;
    float mx = 0.f;
    for (int b = 0; b < 300; b++) {
        m->process(ins, c);
        for (int k = 0; k < kBlock; k++) {
            mx = m->out(0)[k] > mx ? m->out(0)[k] : mx;
            if (k > 0 && m->out(1)[k] > 1.f && m->out(1)[k - 1] < 1.f) eocs++;
        }
    }
    TEST_ASSERT_TRUE(mx > 4.5f);
    TEST_ASSERT_TRUE(eocs >= 10);  // 200 ms, cycles of ~6 ms
    delete m;
}

void test_sco_tuner() {
    std::vector<float> v;
    Module* m = kScoType.create();
    static float b[kBlock];
    const float* ins[1] = {b};
    bool c[1] = {true};
    float ph = 0.f;
    for (int blk = 0; blk < 300; blk++) {
        for (int k = 0; k < kBlock; k++) {
            b[k] = 3.f * sinf(ph);
            ph += 6.2831853f * 440.f / kSampleRate;
        }
        m->process(ins, c);
    }
    TEST_ASSERT_FLOAT_WITHIN(3.f, 440.f, m->monitor(0));
    int n = 0, head = 0;
    const float* d = m->displayBuffer(&n, &head);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(96, n);
    float mx = 0.f;
    for (int i = 0; i < n; i++) mx = fabsf(d[i]) > mx ? fabsf(d[i]) : mx;
    TEST_ASSERT_TRUE(mx > 2.f);
    delete m;
}


// ---- v1.2 Task 13: GRD ----
void test_grd_every_engine_makes_sound() {
    static float tr[kBlock], z[kBlock];
    for (int e = 0; e < 24; e++) {
        Module* m = kGrdType.create();
        TEST_ASSERT_NOT_NULL(m);
        for (int i = 0; i < kGrdType.numParams; i++) m->setParam(i, kGrdType.params[i].def);
        m->setParam(0, (float)e);
        m->setParam(11, 1.f);  // VCA = LPG, FLT = OFF: the bare Plaits voice
        m->setParam(12, 2.f);
        const float* ins[kMaxIns] = {z, tr, z, z, z, z, z, z};
        bool c[kMaxIns] = {false, true};
        double sum = 0;
        int n = 0;
        for (int b = 0; b < 450; b++) {  // 300 ms, a trigger every 100 ms
            constBlock(tr, (b % 150) < 4 ? 5.f : 0.f);
            m->process(ins, c);
            for (int k = 0; k < kBlock; k++) {
                const float v = m->out(0)[k];
                TEST_ASSERT_TRUE(std::isfinite(v));
                if (b >= 30) {
                    sum += (double)v * v;
                    n++;
                }
            }
        }
        char msg[48];
        snprintf(msg, sizeof msg, "engine %d (%s) rms %f", e, kGrdType.params[0].enumNames[e], std::sqrt(sum / n));
        TEST_ASSERT_TRUE_MESSAGE(std::sqrt(sum / n) > 0.004, msg);  // 6OC default patch is quiet (~0.008)
        delete m;
    }
}

void test_grd_pitch_tracks() {
    Module* m = kGrdType.create();
    for (int i = 0; i < kGrdType.numParams; i++) m->setParam(i, kGrdType.params[i].def);
    m->setParam(0, 2.f);   // FM
    m->setParam(3, 0.f);   // harmonics: ratio 1
    m->setParam(4, 0.f);   // timbre: no modulation -> a sine at the note
    static float pit[kBlock], z[kBlock];
    constBlock(pit, 0.f);
    const float* ins[kMaxIns] = {pit, z, z, z, z, z, z, z};
    bool c[kMaxIns] = {true};
    std::vector<float> v;
    for (int b = 0; b < 600; b++) {
        m->process(ins, c);
        if (b > 100) v.insert(v.end(), m->out(0), m->out(0) + kBlock);
    }
    TEST_ASSERT_FLOAT_WITHIN(261.63f * 0.03f, 261.63f, crossingHz(v));
    TEST_ASSERT_EQUAL(8, grdEngineIndex(0));
    TEST_ASSERT_TRUE(kGrdType.ramBytes < 64 * 1024);
    TEST_ASSERT_EQUAL(0, grdEngineIndex(16));
    delete m;
}

// ---- review r2: GRD is a whole Grind voice ----
static double grdRms(Module* m, const float* const* ins, const bool* c, int blocks, float* trig, bool gateHigh) {
    double sum = 0;
    int n = 0;
    for (int b = 0; b < blocks; b++) {
        if (trig) constBlock(trig, gateHigh ? 5.f : 0.f);
        m->process(ins, c);
        if (b < blocks / 2) continue;
        for (int k = 0; k < kBlock; k++) {
            sum += (double)m->out(0)[k] * m->out(0)[k];
            n++;
        }
    }
    return std::sqrt(sum / n);
}

void test_grd_voice_env_and_filter() {
    static float tr[kBlock], z[kBlock];
    Module* m = kGrdType.create();
    for (int i = 0; i < kGrdType.numParams; i++) m->setParam(i, kGrdType.params[i].def);
    const float* ins[kMaxIns] = {z, tr, z, z, z, z, z, z, z};
    bool c[kMaxIns] = {false, true};
    m->setParam(17, 50.f);  // DCY: release 50 ms
    const double on = grdRms(m, ins, c, 300, tr, true);
    const double off = grdRms(m, ins, c, 600, tr, false);
    TEST_ASSERT_TRUE(on > 0.3);
    TEST_ASSERT_TRUE(off < 0.01 * on);
    // No TRG patched: the VCA is open (a drone), so a fresh GRD sounds at once.
    bool free_[kMaxIns] = {};
    TEST_ASSERT_TRUE(grdRms(m, ins, free_, 200, nullptr, false) > 0.3);
    // A low LP cutoff takes most of the saw away; OFF puts it back.
    m->setParam(13, 80.f);
    m->setParam(15, 0.f);
    const double dark = grdRms(m, ins, free_, 200, nullptr, false);
    m->setParam(12, 2.f);
    const double raw = grdRms(m, ins, free_, 200, nullptr, false);
    TEST_ASSERT_TRUE(dark < 0.5 * raw);
    // ENV and LFO outputs
    TEST_ASSERT_TRUE(m->out(3)[0] >= -5.01f && m->out(3)[0] <= 5.01f);
    delete m;
}

void test_grd_timbre_cv_heard_at_default_amount() {  // review r2: TMA 0 used to zero a patched CV
    static float tim[kBlock], z[kBlock];
    auto run = [&](float v) {
        Module* m = kGrdType.create();
        for (int i = 0; i < kGrdType.numParams; i++) m->setParam(i, kGrdType.params[i].def);
        m->setParam(0, 2.f);   // FM engine: timbre = modulation index
        m->setParam(4, 0.f);
        m->setParam(12, 2.f);  // FLT OFF
        constBlock(tim, v);
        const float* ins[kMaxIns] = {z, z, z, z, tim, z, z, z, z};
        bool c[kMaxIns] = {false, false, false, false, true};
        std::vector<float> o;
        for (int b = 0; b < 200; b++) {
            m->process(ins, c);
            if (b > 100) o.insert(o.end(), m->out(0), m->out(0) + kBlock);
        }
        delete m;
        return o;
    };
    const std::vector<float> a = run(0.f), b = run(4.f);
    double d = 0;
    for (size_t i = 0; i < a.size(); i++) d += fabs(a[i] - b[i]);
    TEST_ASSERT_TRUE(d / a.size() > 0.1);
}

void test_grd_short_triggers_at_block_edges() {  // review r3: pulses between chunks were lost
    static float tr[kBlock], z[kBlock];
    for (int at = 0; at < 12; at++) {
        Module* m = kGrdType.create();
        for (int i = 0; i < kGrdType.numParams; i++) m->setParam(i, kGrdType.params[i].def);
        m->setParam(0, 13.f);  // BD: silent until triggered
        m->setParam(11, 2.f);  // VCA DRN: only Plaits decides the level
        const float* ins[kMaxIns] = {z, tr, z, z, z, z, z, z, z};
        bool c[kMaxIns] = {false, true};
        double sum = 0;
        for (int b = 0; b < 60; b++) {
            constBlock(tr, 0.f);
            if (b == 7) tr[at] = tr[at + 1] = 5.f;  // a 2-sample pulse
            m->process(ins, c);
            if (b > 7)
                for (int k = 0; k < kBlock; k++) sum += fabs(m->out(0)[k]);
        }
        char msg[24];
        snprintf(msg, sizeof msg, "pulse at %d", at);
        TEST_ASSERT_TRUE_MESSAGE(sum > 10.0, msg);
        delete m;
    }
}

// ---- KIT ----
// Runs a KIT for nBlocks; returns, per lane (and ACC as lane 8), the sample indices where the
// output rose, and the events() of every block.
struct KitRun {
    std::vector<int> rises[kit::kLanes + 1];
    std::vector<uint32_t> events;
    std::vector<float> acc;
    int high[kit::kLanes] = {};  // samples each trigger lane was high
};
static KitRun runKit(KitModule& k, int nBlocks, const float* run = nullptr, const float* clk = nullptr,
                     int base = 0) {
    KitRun r;
    float prev[kit::kLanes + 1] = {};
    const float* ins[kMaxIns] = {run, clk};
    bool c[kMaxIns] = {run != nullptr, clk != nullptr};
    for (int b = 0; b < nBlocks; b++) {
        k.process(ins, c);
        r.events.push_back(k.events());
        for (int i = 0; i < kBlock; i++) {
            for (int o = 0; o <= kit::kLanes; o++) {
                const float v = k.out(o)[i];
                if (v > 0.f && prev[o] <= 0.f) r.rises[o].push_back(base + b * kBlock + i);
                prev[o] = v;
                if (o < kit::kLanes && v > 0.f) r.high[o]++;
            }
            r.acc.push_back(k.out(kit::kAccOut)[i]);
        }
    }
    return r;
}

void test_kit_one_shot_pass() {
    KitModule k;
    k.setParam(kit::kLen, 3.f);
    k.setParam(kit::kGap, 150.f);  // 7200 samples
    KitRun idle = runKit(k, 10);
    for (int o = 0; o < kit::kLanes; o++) TEST_ASSERT_EQUAL(0, (int)idle.rises[o].size());
    TEST_ASSERT_FALSE(k.running());
    k.start();
    KitRun r = runKit(k, 900 * 48 / kBlock);  // 900 ms
    // default lanes: lane l plays step l
    TEST_ASSERT_EQUAL(1, (int)r.rises[0].size());
    TEST_ASSERT_EQUAL(0, r.rises[0][0]);
    TEST_ASSERT_EQUAL(1, (int)r.rises[1].size());
    TEST_ASSERT_EQUAL(7200, r.rises[1][0]);
    TEST_ASSERT_EQUAL(1, (int)r.rises[2].size());
    TEST_ASSERT_EQUAL(14400, r.rises[2][0]);
    for (int o = 3; o < kit::kLanes; o++) TEST_ASSERT_EQUAL(0, (int)r.rises[o].size());
    TEST_ASSERT_FALSE(k.running());
    // events: lane bits on the firing blocks, a step edge there and at the pass end (21600)
    TEST_ASSERT_EQUAL_UINT32(kit::kStepEdge | 1u, r.events[0]);
    TEST_ASSERT_EQUAL_UINT32(kit::kStepEdge | 2u, r.events[7200 / kBlock]);
    TEST_ASSERT_EQUAL_UINT32(kit::kStepEdge, r.events[21600 / kBlock]);
    int edges = 0;
    for (uint32_t e : r.events) edges += (e & kit::kStepEdge) != 0;
    TEST_ASSERT_EQUAL(4, edges);
    TEST_ASSERT_EQUAL(kit::kTrigSamples, r.high[0]);  // 5 ms triggers
    TEST_ASSERT_EQUAL_FLOAT(-1.f, k.monitor(0));       // idle: no playhead
}

void test_kit_stacks_loops_and_accents() {
    KitModule k;
    k.setParam(kit::kL1 + 0, (float)0b11);  // T1 on steps 1 and 2
    k.setParam(kit::kL1 + 1, (float)0b10);  // T2 on step 2: stacked with T1
    k.setParam(kit::kAccLane, (float)0b10);
    k.setParam(kit::kAcc, 0.5f);
    k.setParam(kit::kLen, 2.f);
    k.setParam(kit::kGap, 200.f);  // 9600 samples
    k.setParam(kit::kMode, 1.f);   // loop
    k.start();
    KitRun r = runKit(k, 4 * 9600 / kBlock + 1);
    // T1: 0, 9600, 19200, 28800, 38400; T2 with T1 on the 2nd step of each loop
    TEST_ASSERT_EQUAL(5, (int)r.rises[0].size());
    TEST_ASSERT_EQUAL(19200, r.rises[0][2]);
    TEST_ASSERT_EQUAL(2, (int)r.rises[1].size());
    TEST_ASSERT_EQUAL(9600, r.rises[1][0]);
    TEST_ASSERT_EQUAL(28800, r.rises[1][1]);
    TEST_ASSERT_EQUAL_UINT32(kit::kStepEdge | 3u, r.events[9600 / kBlock]);
    TEST_ASSERT_TRUE(k.running());
    // ACC: 2.5 V through the accented step, 0 on the other
    TEST_ASSERT_EQUAL_FLOAT(0.f, r.acc[100]);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, r.acc[9600 + 5000]);
    TEST_ASSERT_EQUAL_FLOAT(0.f, r.acc[19200 + 100]);
    // the step shows for the playhead
    TEST_ASSERT_EQUAL_FLOAT(0.f, k.monitor(0));
}

void test_kit_run_and_clock_inputs() {
    KitModule k;
    k.setParam(kit::kLen, 3.f);
    float lo[kBlock], hi[kBlock];
    constBlock(lo, 0.f);
    constBlock(hi, 5.f);
    // RUN rising starts a pass
    runKit(k, 2, lo);
    KitRun a = runKit(k, 1, hi, nullptr);
    TEST_ASSERT_EQUAL(1, (int)a.rises[0].size());
    TEST_ASSERT_TRUE(k.running());
    // CLK patched: no step without a clock rise, however long the gap
    KitModule c;
    c.setParam(kit::kLen, 3.f);
    c.setParam(kit::kGap, 150.f);
    c.start();
    KitRun b = runKit(c, 400, nullptr, lo);  // 266 ms, no clock
    TEST_ASSERT_EQUAL(1, (int)b.rises[0].size());
    TEST_ASSERT_EQUAL(0, (int)b.rises[1].size());
    runKit(c, 1, nullptr, hi);
    runKit(c, 1, nullptr, lo);
    KitRun d = runKit(c, 1, nullptr, hi);  // 2nd rise: step 3
    TEST_ASSERT_EQUAL(1, (int)d.rises[2].size());
    runKit(c, 1, nullptr, lo);
    KitRun e = runKit(c, 1, nullptr, hi);  // past the last step: the pass ends
    TEST_ASSERT_EQUAL_UINT32(kit::kStepEdge, e.events[0]);
    TEST_ASSERT_FALSE(c.running());
}

void test_kit_worst_step() {
    Patch p;
    uint16_t id = 0;
    TEST_ASSERT_EQUAL(PatchError::None, p.addModule(kKitType, 0, 0, &id));
    ModuleRec& r = *p.find(id);
    r.params[kit::kLen] = 4.f;
    const uint32_t cost[kit::kLanes] = {100, 200, 300, 400, 5000, 0, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(400, kitWorstStep(r, cost));  // lane 4 plays step 5: past LEN
    r.params[kit::kL1 + 0] = (float)0b1001;                 // T1 also on step 4, with T4
    TEST_ASSERT_EQUAL_UINT32(500, kitWorstStep(r, cost));
    r.bypass = 1;
    TEST_ASSERT_EQUAL_UINT32(0, kitWorstStep(r, cost));
}

// ---- v1.4 modules ----
void test_arp_patterns() {
    int n[24];
    TEST_ASSERT_EQUAL(3, arpPattern(0, 0, 1, n, 24));  // MAJ UP 1 oct
    TEST_ASSERT_EQUAL(0, n[0]); TEST_ASSERT_EQUAL(4, n[1]); TEST_ASSERT_EQUAL(7, n[2]);
    TEST_ASSERT_EQUAL(6, arpPattern(1, 0, 2, n, 24));  // MIN UP 2 oct: 0 3 7 12 15 19
    TEST_ASSERT_EQUAL(19, n[5]);
    TEST_ASSERT_EQUAL(3, arpPattern(0, 1, 1, n, 24));  // DN: 7 4 0
    TEST_ASSERT_EQUAL(7, n[0]); TEST_ASSERT_EQUAL(0, n[2]);
    TEST_ASSERT_EQUAL(4, arpPattern(0, 2, 1, n, 24));  // UPDN: 0 4 7 4, ends not doubled
    TEST_ASSERT_EQUAL(4, n[3]);
    TEST_ASSERT_EQUAL(1, arpPattern(6, 2, 1, n, 24));  // OCT chord, 1 oct: the root alone
}

void test_arp_clocked_steps() {
    Module* m = kArpType.create();  // defaults: MAJ, UP, 1 octave; GATE unpatched: runs
    std::vector<float> p = clocked(m, 6, 0);
    const float want[] = {0.f, 4.f / 12.f, 7.f / 12.f, 0.f, 4.f / 12.f, 7.f / 12.f};
    for (int k = 0; k < 6; k++) TEST_ASSERT_FLOAT_WITHIN(1e-4f, want[k], p[k]);
    std::vector<float> g = clocked(m, 2, 1);  // the gate follows the clock
    TEST_ASSERT_EQUAL_FLOAT(kGateHighVolts, g[0]);
    delete m;
}

void test_arp_gate_holds_and_restarts() {
    Module* m = kArpType.create();
    static float ck[kBlock], z[kBlock], gt[kBlock];
    const float* ins[kMaxIns] = {ck, z, gt, z, z, z, z, z};
    bool c[kMaxIns] = {true, false, true};
    auto step = [&](float gate) {  // one clock pulse with GATE at `gate`
        constBlock(gt, gate);
        constBlock(ck, 5.f);
        for (int b = 0; b < 4; b++) m->process(ins, c);
        const float pit = m->out(0)[kBlock - 1], g = m->out(1)[kBlock - 1];
        constBlock(ck, 0.f);
        for (int b = 0; b < 4; b++) m->process(ins, c);
        return std::make_pair(pit, g);
    };
    TEST_ASSERT_EQUAL_FLOAT(0.f, step(0.f).second);  // no gate: no notes
    step(5.f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.f / 12.f, step(5.f).first);
    step(0.f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.f, step(5.f).first);  // a new gate starts at the root
    delete m;
}

static std::vector<float> trigRun(Module* m, int blocks, int trigIn = 0) {
    static float tr[kBlock], z[kBlock];
    const float* ins[kMaxIns] = {z, z, z, z, z, z, z, z};
    ins[trigIn] = tr;
    bool c[kMaxIns] = {};
    c[trigIn] = true;
    std::vector<float> r;
    for (int b = 0; b < blocks; b++) {
        constBlock(tr, b < 2 ? 5.f : 0.f);
        m->process(ins, c);
        r.insert(r.end(), m->out(0), m->out(0) + kBlock);
    }
    return r;
}
static float windowRms(const std::vector<float>& s, size_t from, size_t to) {
    double e = 0.0;
    for (size_t i = from; i < to && i < s.size(); i++) e += (double)s[i] * s[i];
    return (float)sqrt(e / (double)(to - from));
}

void test_clp_bursts_then_tail() {
    Module* m = kClpType.create();
    m->setParam(2, 10.f);  // SPR 10 ms between bursts
    m->setParam(1, 60.f);  // DEC 60 ms: gone well before 250 ms
    const std::vector<float> s = trigRun(m, 30 * 48 / kBlock * 10);  // ~300 ms
    // three bursts: each 10 ms window starts loud and dips before the next
    const size_t ms = 48;
    for (int k = 0; k < 3; k++) {
        const float start = windowRms(s, k * 10 * ms, k * 10 * ms + 2 * ms), end = windowRms(s, k * 10 * ms + 7 * ms, k * 10 * ms + 10 * ms);
        TEST_ASSERT_TRUE_MESSAGE(start > 2.f * end, "a burst dips before the next");
    }
    char msg[80];
    snprintf(msg, sizeof msg, "tail %.3f late %.3f", (double)windowRms(s, 30 * ms, 60 * ms), (double)windowRms(s, 250 * ms, 290 * ms));
    TEST_ASSERT_TRUE_MESSAGE(windowRms(s, 30 * ms, 60 * ms) > 0.2f, msg);    // the tail rings
    TEST_ASSERT_TRUE(windowRms(s, 30 * ms, 60 * ms) > 4.f * windowRms(s, 250 * ms, 290 * ms));  // and dies
    delete m;
}

void test_prc_modes() {
    for (int mode = 0; mode < 5; mode++) {
        Module* m = kPrcType.create();
        m->setParam(0, (float)mode);
        m->setParam(2, 0.f);  // the shortest DEC: every mode is gone by 300 ms
        const std::vector<float> s = trigRun(m, 40 * 48 / kBlock * 10);
        const float early = windowRms(s, 0, 30 * 48), late = windowRms(s, 300 * 48, 390 * 48);
        TEST_ASSERT_TRUE_MESSAGE(early > 0.3f, "sounds");
        char msg[64];
        snprintf(msg, sizeof msg, "decays: mode %d early %.3f late %.3f", mode, (double)early, (double)late);
        TEST_ASSERT_TRUE_MESSAGE(early > 5.f * late, msg);
        for (float v : s) TEST_ASSERT_TRUE(fabsf(v) < 12.f);
        delete m;
    }
    // TOM sits near its 110 Hz after the sweep; TUN +12 doubles it
    for (int oct = 0; oct < 2; oct++) {
        Module* m = kPrcType.create();
        m->setParam(1, oct ? 12.f : 0.f);
        m->setParam(2, 1.f);  // long decay
        const std::vector<float> s = trigRun(m, 500);  // 333 ms
        const std::vector<float> tail(s.begin() + 4800, s.begin() + 14400);
        TEST_ASSERT_FLOAT_WITHIN(oct ? 24.f : 12.f, oct ? 220.f : 110.f, crossingHz(tail));
        delete m;
    }
}

void test_vow_formants_and_filter() {
    float f[3];
    vowFormants(0.f, f);  // A
    TEST_ASSERT_FLOAT_WITHIN(1.f, 730.f, f[0]);
    TEST_ASSERT_FLOAT_WITHIN(1.f, 1090.f, f[1]);
    vowFormants(2.f, f);  // I
    TEST_ASSERT_FLOAT_WITHIN(1.f, 270.f, f[0]);
    vowFormants(0.5f, f);  // halfway A -> E
    TEST_ASSERT_FLOAT_WITHIN(1.f, (730.f + 530.f) / 2.f, f[0]);
    // A passes its F1 far more than a frequency between the formants
    auto gainAt = [](float hz) {
        Module* m = kVowType.create();
        const float r = sineRms(*m, hz, 2.f, 200);
        delete m;
        return r;
    };
    TEST_ASSERT_TRUE(gainAt(730.f) > 3.f * gainAt(5000.f));
    // review: a 5 V sine on F1 passes about at unity, nowhere near the +-10 V clamp, at any RES
    for (float res : {0.f, 0.6f, 1.f}) {
        Module* m = kVowType.create();
        m->setParam(2, res);
        static float in[kBlock];
        const float* ins[2] = {in, nullptr};
        const bool c[2] = {true, false};
        float ph = 0.f, peak = 0.f;
        for (int b = 0; b < 300; b++) {
            for (int i = 0; i < kBlock; i++, ph += 730.f / kSampleRate) in[i] = 5.f * sinf(6.2831853f * ph);
            m->process(ins, c);
            if (b > 150)
                for (int i = 0; i < kBlock; i++) peak = fabsf(m->out(0)[i]) > peak ? fabsf(m->out(0)[i]) : peak;
        }
        TEST_ASSERT_TRUE_MESSAGE(peak < 9.f && peak > 2.f, "VOW at F1: no clamp, no collapse");
        delete m;
    }
}

// review: with GATE patched and CLK not, a new gate sounds at once (not at the next RATE tick)
void test_arp_gate_sounds_at_once() {
    Module* m = kArpType.create();
    m->setParam(3, 1.f);  // RATE 1 Hz: the next tick is a second away
    static float z[kBlock], gt[kBlock];
    const float* ins[kMaxIns] = {z, z, gt, z, z, z, z, z};
    bool c[kMaxIns] = {false, false, true};
    constBlock(gt, 0.f);
    for (int b = 0; b < 100; b++) m->process(ins, c);
    constBlock(gt, 5.f);
    m->process(ins, c);
    m->process(ins, c);
    TEST_ASSERT_EQUAL_FLOAT(kGateHighVolts, m->out(1)[kBlock - 1]);
    delete m;
}

void test_eq3_shapes() {
    auto gain = [](float lo, float mid, float hi, float hz) {
        Module* m = kEq3Type.create();
        m->setParam(0, lo);
        m->setParam(1, mid);
        m->setParam(3, hi);
        const float r = sineRms(*m, hz, 2.f, 300) / (2.f / sqrtf(2.f));
        delete m;
        return 20.f * log10f(r);
    };
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.f, gain(0.f, 0.f, 0.f, 1000.f));  // flat at 0 dB
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 12.f, gain(12.f, 0.f, 0.f, 50.f));  // low shelf up
    TEST_ASSERT_FLOAT_WITHIN(1.5f, -12.f, gain(0.f, 0.f, -12.f, 12000.f));  // high shelf down
    TEST_ASSERT_FLOAT_WITHIN(1.f, 9.f, gain(0.f, 9.f, 0.f, 1000.f));   // the mid peak at FRQ
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_registry);
    RUN_TEST(test_osc_pitch);
    RUN_TEST(test_osc_amplitude);
    RUN_TEST(test_vca_normalled);
    RUN_TEST(test_vcf_lowpass);
    RUN_TEST(test_vcf_hp);
    RUN_TEST(test_vcf_cut_cv);
    RUN_TEST(test_env_shape);
    RUN_TEST(test_env_retrig);
    RUN_TEST(test_env_retrig_gate_low);
    RUN_TEST(test_lfo_rate);
    RUN_TEST(test_keys_pitch_gate);
    RUN_TEST(test_keys_glide);
    RUN_TEST(test_keys_retrigger_gap);
    RUN_TEST(test_keys_quick_tap);
    RUN_TEST(test_out_volume_select);
    RUN_TEST(test_out_trim);
    RUN_TEST(test_out_bounded);
    RUN_TEST(test_pool_exhaustion);
    RUN_TEST(test_dly_create_refused);
    RUN_TEST(test_dly_echo);
    RUN_TEST(test_dly_feedback_bounded);
    RUN_TEST(test_dly_tail_decays_to_zero);
    RUN_TEST(test_dly_mix_smoothed);
    RUN_TEST(test_mix_sums_levels);
    RUN_TEST(test_mix_unconnected_silent);
    RUN_TEST(test_atn_inverts_and_offsets);
    RUN_TEST(test_atn_no_input_is_dc);
    RUN_TEST(test_snh_holds_on_trigger);
    RUN_TEST(test_snh_random_without_input);
    RUN_TEST(test_snh_slew);
    RUN_TEST(test_snh_holds_without_trigger);
    RUN_TEST(test_clk_120bpm_period);
    RUN_TEST(test_clk_div3);
    RUN_TEST(test_clk_external_clock);
    RUN_TEST(test_clk_reset);
    RUN_TEST(test_selector_cv);
    RUN_TEST(test_selector_step);
    RUN_TEST(test_mux_picks_input);
    RUN_TEST(test_mux_switch_is_smooth);
    RUN_TEST(test_dmx_routes);
    RUN_TEST(test_tilt_module_scales);
    RUN_TEST(test_tilt_module_clamps);
    RUN_TEST(test_tilt_module_smooths);
    RUN_TEST(test_clk_reset_while_high_retriggers);
    RUN_TEST(test_selector_cv_hysteresis);
    RUN_TEST(test_tilt_smooth_minimum);
    RUN_TEST(test_all_types_sane);
    RUN_TEST(test_all_types_round_trip);
    RUN_TEST(test_all_types_unique_keys);
    RUN_TEST(test_rev_tail_decays);
    RUN_TEST(test_pool_shared_dly_rev);
    RUN_TEST(test_psh_octave_up);
    RUN_TEST(test_lad_lowpasses);
    RUN_TEST(test_vcf_bp_mode);
    RUN_TEST(test_fld_folds_above_threshold);
    RUN_TEST(test_cru_quantizes);
    RUN_TEST(test_lpg_closes_after_decay);
    RUN_TEST(test_ring_multiplies);
    RUN_TEST(test_xfd_ends);
    RUN_TEST(test_dyn_reduces_loud);
    RUN_TEST(test_enf_tracks_level);
    RUN_TEST(test_slw_rise_time);
    RUN_TEST(test_noi_kinds_differ);
    RUN_TEST(test_kck_trig_makes_sound);
    RUN_TEST(test_str_pluck_decays);
    RUN_TEST(test_ssw_pitch);
    RUN_TEST(test_vso_pitch);
    RUN_TEST(test_fmo_index_zero_is_sine);
    RUN_TEST(test_quantize_major);
    RUN_TEST(test_seq_steps_and_gates);
    RUN_TEST(test_trg_lanes);
    RUN_TEST(test_euc_e38);
    RUN_TEST(test_tur_locked_loops);
    RUN_TEST(test_lgc_xor);
    RUN_TEST(test_lfo_old_waves_same);
    RUN_TEST(test_ad_loop);
    RUN_TEST(test_sco_tuner);
    RUN_TEST(test_grd_every_engine_makes_sound);
    RUN_TEST(test_grd_voice_env_and_filter);
    RUN_TEST(test_grd_short_triggers_at_block_edges);
    RUN_TEST(test_grd_timbre_cv_heard_at_default_amount);
    RUN_TEST(test_grd_pitch_tracks);
    RUN_TEST(test_kit_one_shot_pass);
    RUN_TEST(test_kit_stacks_loops_and_accents);
    RUN_TEST(test_kit_run_and_clock_inputs);
    RUN_TEST(test_kit_worst_step);
    RUN_TEST(test_arp_patterns);
    RUN_TEST(test_arp_clocked_steps);
    RUN_TEST(test_arp_gate_holds_and_restarts);
    RUN_TEST(test_clp_bursts_then_tail);
    RUN_TEST(test_prc_modes);
    RUN_TEST(test_vow_formants_and_filter);
    RUN_TEST(test_eq3_shapes);
    RUN_TEST(test_arp_gate_sounds_at_once);
    return UNITY_END();
}
