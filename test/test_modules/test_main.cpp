#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "core/registry.h"
#include "core/modules/osc.h"
#include "core/modules/vca.h"

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

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_registry);
    RUN_TEST(test_osc_pitch);
    RUN_TEST(test_osc_amplitude);
    RUN_TEST(test_vca_normalled);
    return UNITY_END();
}
