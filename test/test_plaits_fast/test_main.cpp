// v1.3 Task 4: the Plaits / stmlib speed edits (reciprocals instead of divides, unrolled loops)
// must match the unmodified classes in ref_plaits.h. Natively FastRecip is 1/x, so these check
// the rewritten expressions; recip0.s itself is checked on the device by the bench.
#include <unity.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "plaits/dsp/oscillator/grainlet_oscillator.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"
#include "plaits/dsp/physical_modelling/resonator.h"
#include "ref_plaits.h"

void setUp() {}
void tearDown() {}

namespace {

void expectClose(const char* what, const std::vector<float>& a, const std::vector<float>& b, float tol) {
    float peak = 1e-6f, worst = 0.f;
    for (size_t i = 0; i < b.size(); i++) {
        peak = fmaxf(peak, fabsf(b[i]));
        worst = fmaxf(worst, fabsf(a[i] - b[i]));
    }
    char m[160];
    snprintf(m, sizeof m, "%s: rel err %.3g (tol %.3g, peak %.3g)", what, (double)(worst / peak), (double)tol,
             (double)peak);
    if (!(peak > 0.01f)) TEST_FAIL_MESSAGE((std::string(m) + ": silent").c_str());
    if (!(worst / peak <= tol)) TEST_FAIL_MESSAGE(m);
    TEST_MESSAGE(m);
}

constexpr int kN = 12;  // Plaits' block

}  // namespace

void test_soft_limit_matches() {
    std::vector<float> a, b;
    for (int i = -3000; i <= 3000; i++) {
        const float x = (float)i * 0.001f;
        a.push_back(stmlib::SoftLimit(x));
        b.push_back(stmlib_ref::SoftLimit(x));
    }
    expectClose("SoftLimit", a, b, 1e-6f);
}

void test_svf_set_f_q_matches() {
    stmlib::Svf a;
    stmlib_ref::Svf b;
    a.Init();
    b.Init();
    std::vector<float> oa, ob;
    for (int i = 0; i < 20000; i++) {
        const float f = 0.002f + 0.2f * (0.5f + 0.5f * sinf((float)i * 0.001f));
        const float q = 0.5f + 8.f * (0.5f + 0.5f * sinf((float)i * 0.0003f));
        a.set_f_q<stmlib::FREQUENCY_FAST>(f, q);
        b.set_f_q<stmlib::FREQUENCY_FAST>(f, q);
        const float x = (i % 200) < 100 ? 0.3f : -0.3f;
        oa.push_back(a.Process<stmlib::FILTER_MODE_BAND_PASS>(x));
        ob.push_back(b.Process<stmlib::FILTER_MODE_BAND_PASS>(x));
    }
    expectClose("stmlib Svf set_f_q", oa, ob, 1e-5f);
}

void test_resonator_svf_matches() {
    plaits::ResonatorSvf<4> a;
    plaits_ref::ResonatorSvf<4> b;
    a.Init();
    b.Init();
    const float f[4] = {0.01f, 0.031f, 0.07f, 0.2f}, q[4] = {200.f, 120.f, 40.f, 3.f}, g[4] = {1.f, 0.7f, 0.5f, 0.2f};
    std::vector<float> oa, ob;
    float in[kN], xa[kN], xb[kN];
    for (int blk = 0; blk < 2000; blk++) {
        for (int i = 0; i < kN; i++) in[i] = (blk % 100 == 0 && i == 0) ? 1.f : 0.f;
        for (int i = 0; i < kN; i++) xa[i] = xb[i] = 0.f;
        a.Process<stmlib::FILTER_MODE_BAND_PASS, true>(f, q, g, in, xa, kN);
        b.Process<stmlib::FILTER_MODE_BAND_PASS, true>(f, q, g, in, xb, kN);
        oa.insert(oa.end(), xa, xa + kN);
        ob.insert(ob.end(), xb, xb + kN);
    }
    expectClose("ResonatorSvf (modal modes)", oa, ob, 1e-4f);
}

void test_variable_shape_oscillator_matches() {
    plaits::VariableShapeOscillator a;
    plaits_ref::VariableShapeOscillator b;
    a.Init();
    b.Init();
    std::vector<float> oa, ob;
    float xa[kN], xb[kN];
    for (int blk = 0; blk < 4000; blk++) {
        const float t = (float)blk / 4000.f;
        const float master = 0.002f + 0.01f * t, freq = 0.004f + 0.03f * t;
        const float pw = 0.5f + 0.45f * sinf(t * 20.f), shape = t;
        a.Render<true, false>(master, freq, pw, shape, 0.f, xa, kN);
        b.Render<true, false>(master, freq, pw, shape, 0.f, xb, kN);
        oa.insert(oa.end(), xa, xa + kN);
        ob.insert(ob.end(), xb, xb + kN);
    }
    expectClose("VariableShapeOscillator (sync, moving PW)", oa, ob, 1e-4f);
}

void test_grainlet_matches() {
    plaits::GrainletOscillator a;
    plaits_ref::GrainletOscillator b;
    a.Init();
    b.Init();
    std::vector<float> oa, ob;
    float xa[kN], xb[kN];
    for (int blk = 0; blk < 4000; blk++) {
        const float t = (float)blk / 4000.f;
        const float shape = t, bleed = 0.5f * (1.f - t);  // all three carrier shapes
        a.Render(0.005f, 0.02f + 0.05f * t, shape, bleed, xa, kN);
        b.Render(0.005f, 0.02f + 0.05f * t, shape, bleed, xb, kN);
        oa.insert(oa.end(), xa, xa + kN);
        ob.insert(ob.end(), xb, xb + kN);
    }
    expectClose("GrainletOscillator", oa, ob, 1e-4f);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_soft_limit_matches);
    RUN_TEST(test_svf_set_f_q_matches);
    RUN_TEST(test_resonator_svf_matches);
    RUN_TEST(test_variable_shape_oscillator_matches);
    RUN_TEST(test_grainlet_matches);
    return UNITY_END();
}
