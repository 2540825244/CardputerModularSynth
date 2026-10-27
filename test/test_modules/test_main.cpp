#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "core/registry.h"
#include "core/modules/osc.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"
#include "core/modules/env.h"
#include "core/modules/lfo.h"
#include "core/modules/keys.h"
#include "core/modules/out.h"
#include "core/modules/dly.h"
#include "core/delay_pool.h"
#include "core/dsp_util.h"

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
    const float* ins[1] = {r};
    const bool conn[1] = {true};
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
    const float* ins[2] = {in, nullptr};
    const bool conn[2] = {true, false};
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
    const float* ins[2] = {in, nullptr};
    const bool conn[2] = {true, false};
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
    const float* ins[2] = {in, nullptr};
    const bool conn[2] = {true, false};
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
    const float* ins[2] = {in, nullptr};
    const bool conn[2] = {true, false};
    m->process(ins, conn);
    m->setParam(2, 1.f);  // wet is silent, so mix 0 -> 1 must fade the dry out, not cut
    m->process(ins, conn);
    TEST_ASSERT_TRUE(m->out(0)[0] > 3.9f);
    TEST_ASSERT_TRUE(m->out(0)[kBlock - 1] > 3.f);
    delete m;
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
    RUN_TEST(test_out_volume_select);
    RUN_TEST(test_out_bounded);
    RUN_TEST(test_pool_exhaustion);
    RUN_TEST(test_dly_create_refused);
    RUN_TEST(test_dly_echo);
    RUN_TEST(test_dly_feedback_bounded);
    RUN_TEST(test_dly_tail_decays_to_zero);
    RUN_TEST(test_dly_mix_smoothed);
    return UNITY_END();
}
