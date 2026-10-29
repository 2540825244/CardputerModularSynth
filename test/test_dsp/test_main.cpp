#include <unity.h>
#include <cmath>
#include <cstdio>
#include "core/dsp_util.h"
#include "core/ladder.h"
#include "core/note_stack.h"
#include "core/scales.h"
#include <cstring>
#include "Filters/ladder.h"

void setUp() {}
void tearDown() {}

void test_fast_exp2_accuracy() {
    for (int i = -1200; i <= 1200; i++) {
        const float x = (float)i * 0.01f;
        const float e = fabsf(fastExp2(x) / exp2f(x) - 1.f);
        if (e >= 1e-4f) TEST_FAIL_MESSAGE("fastExp2 error too large");
    }
}

void test_slew_time() {
    Slew s;
    s.setTimeMs(10.f, 48000.f);
    float y = 0.f;
    for (int i = 0; i < 400; i++) y = s.process(1.f);
    TEST_ASSERT_TRUE(y < 0.63f);
    for (int i = 400; i < 480; i++) y = s.process(1.f);
    TEST_ASSERT_TRUE(y > 0.63f);
    Slew p;
    p.setTimeMs(0.f, 48000.f);
    TEST_ASSERT_EQUAL_FLOAT(0.7f, p.process(0.7f));
}

void test_gate_hysteresis() {
    GateDetector g;
    const float in[] = {0.f, 1.2f, 0.8f, 0.4f, 1.2f};
    const bool high[] = {false, true, true, false, true};
    const bool rose[] = {false, true, false, false, true};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(high[i], g.process(in[i]));
        TEST_ASSERT_EQUAL(rose[i], g.rose);
    }
}

void test_soft_clip_bounds() {
    for (float x = -1e6f; x <= 1e6f; x += 997.f) TEST_ASSERT_TRUE(fabsf(softClip(x)) <= 1.f);
    for (float x = -10.f; x <= 10.f; x += 0.01f) {
        TEST_ASSERT_TRUE(fabsf(softClip(x)) <= 1.f);
        TEST_ASSERT_EQUAL_FLOAT(-softClip(x), softClip(-x));
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, softClip(0.25f));
}

// Ladder::ProcessBlock is Process() per sample with the state in registers: bit-identical.
void test_ladder_block_matches_process() {
    for (int mode = 0; mode < 6; mode++) {
        Ladder a, b;
        a.Init(48000.f);
        b.Init(48000.f);
        a.SetFilterMode((Ladder::Mode)mode);
        b.SetFilterMode((Ladder::Mode)mode);
        a.SetRes(1.2f);
        b.SetRes(1.2f);
        uint32_t seed = 5;
        for (int blk = 0; blk < 400; blk++) {
            const float hz = 200.f + 50.f * (float)(blk % 80);
            a.SetFreq(hz);
            b.SetFreq(hz);
            float x[8], y[8];
            for (int i = 0; i < 8; i++) {
                seed = seed * 1664525u + 1013904223u;
                x[i] = y[i] = ((float)(seed >> 8) / 16777216.f - 0.5f) * 1.6f;
            }
            for (float& v : x) v = a.Process(v);
            b.ProcessBlock(y, 8);
            for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_HEX32(*(uint32_t*)&x[i], *(uint32_t*)&y[i]);
        }
    }
}

// quantize() without floorf and with a stepped pitch class: the old code's results.
static float oldQuantize(float volts, int scale, int root) {
    const unsigned m = scaleMask(scale);
    const float semi = volts * 12.f - (float)root;
    const int base = (int)floorf(semi);
    int best = base;
    float bestD = 1e9f;
    for (int c = base - 6; c <= base + 7; c++) {
        const int pc = ((c % 12) + 12) % 12;
        if (!((m >> pc) & 1)) continue;
        const float d = fabsf((float)c - semi);
        if (d < bestD - 1e-6f || (fabsf(d - bestD) < 1e-6f && c > best)) {
            bestD = d;
            best = c;
        }
    }
    return (float)(best + root) / 12.f;
}

void test_quantize_matches_old() {
    for (int scale = 0; scale < scaleCount(); scale++)
        for (int root = 0; root < 12; root += 5)
            for (int i = -60000; i <= 60000; i += 7) {
                const float v = (float)i * 0.0001f;  // -6..+6 V
                TEST_ASSERT_EQUAL_FLOAT(oldQuantize(v, scale, root), quantize(v, scale, root));
            }
}

// sanitizeBlock must give exactly sanitize() per value, and the peak of |result|.
void test_sanitize_block_matches() {
    const float vals[] = {0.f, -0.f, 1e-40f, -1e-40f, 1.5f, -1.5f, 49.999f, 50.f, -50.f, 50.0001f, -50.0001f,
                          1e9f, -1e9f, 3.0e38f, -3.0e38f, 3.0001e38f, -3.0001e38f, 3.4e38f, INFINITY, -INFINITY,
                          NAN, -NAN, 1e-30f, 7.f};
    const int n = sizeof vals / sizeof vals[0];
    float a[n];
    for (int i = 0; i < n; i++) a[i] = vals[i];
    const float pk = sanitizeBlock(a, n);
    float want = 0.f;
    for (int i = 0; i < n; i++) {
        const float s = sanitize(vals[i]);
        uint32_t x, y;
        memcpy(&x, &a[i], 4);
        memcpy(&y, &s, 4);
        TEST_ASSERT_EQUAL_HEX32(y, x);  // bit-identical, signed zeros included
        want = fabsf(s) > want ? fabsf(s) : want;
    }
    TEST_ASSERT_EQUAL_FLOAT(want, pk);
}

void test_sanitize() {
    TEST_ASSERT_EQUAL_FLOAT(0.f, sanitize(NAN));
    TEST_ASSERT_EQUAL_FLOAT(0.f, sanitize(INFINITY));
    TEST_ASSERT_EQUAL_FLOAT(50.f, sanitize(1e9f));
    TEST_ASSERT_EQUAL_FLOAT(-50.f, sanitize(-1e9f));
    TEST_ASSERT_EQUAL_FLOAT(1.5f, sanitize(1.5f));
}

void test_note_stack() {
    NoteStack n;
    TEST_ASSERT_EQUAL(-1, n.current());
    n.press(60);
    n.press(64);
    n.release(64);
    TEST_ASSERT_EQUAL(60, n.current());
    n.release(60);
    TEST_ASSERT_EQUAL(-1, n.current());
    for (int i = 0; i < 9; i++) n.press(40 + i);
    TEST_ASSERT_EQUAL(48, n.current());
    for (int i = 8; i >= 1; i--) n.release(40 + i);
    TEST_ASSERT_EQUAL(-1, n.current());  // oldest (40) was dropped
    n.clear();
    n.press(50);
    n.press(50);
    n.release(50);
    TEST_ASSERT_EQUAL(-1, n.current());
}

void test_slew_reaches_target_exactly() {
    Slew s;
    s.setTimeMs(2.f, kSampleRate);
    s.y = 1.f;
    for (int i = 0; i < 48000; i++) s.process(0.f);
    TEST_ASSERT_EQUAL_FLOAT(0.f, s.y);  // no subnormal tail
    TEST_ASSERT_TRUE(s.y == 0.f);
}

// The fast ladder must match DaisySP's on every mode, cutoff and resonance (same model).
void test_ladder_matches_daisysp() {
    const float cuts[] = {80.f, 800.f, 5000.f, 17000.f};
    const float ress[] = {0.f, 0.6f, 1.8f};
    for (int mode = 0; mode < 6; mode++)
        for (float hz : cuts)
            for (float res : ress) {
                daisysp::LadderFilter d;
                Ladder f;
                d.Init(48000.f);
                f.Init(48000.f);
                d.SetFilterMode((daisysp::LadderFilter::FilterMode)mode);
                f.SetFilterMode((Ladder::Mode)mode);
                d.SetInputDrive(2.5f);
                f.SetInputDrive(2.5f);
                d.SetFreq(hz);
                f.SetFreq(hz);
                d.SetRes(res);
                f.SetRes(res);
                uint32_t seed = 1;
                float worst = 0.f;
                for (int i = 0; i < 4000; i++) {
                    seed = seed * 1664525u + 1013904223u;
                    const float x = ((float)(seed >> 8) / 16777216.f - 0.5f) * 1.6f + ((i / 60) & 1 ? 0.4f : -0.4f);
                    const float e = fabsf(d.Process(x) - f.Process(x));
                    worst = e > worst ? e : worst;
                }
                if (!(worst < 1e-4f)) {
                    char m[96];
                    snprintf(m, sizeof m, "mode %d %.0f Hz res %.1f: diff %g", mode, hz, res, worst);
                    TEST_FAIL_MESSAGE(m);
                }
            }
}

void test_soft_clip_bounded() {
    for (int i = -4000; i <= 4000; i++) {
        const float x = (float)i * 0.001f;
        const float y = softClip(x);
        TEST_ASSERT_TRUE(y <= 1.f && y >= -1.f);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, -y, softClip(-x));
    }
    TEST_ASSERT_EQUAL_FLOAT(0.f, sanitize(NAN));
    TEST_ASSERT_EQUAL_FLOAT(0.f, sanitize(INFINITY));
    TEST_ASSERT_EQUAL_FLOAT(50.f, sanitize(1e6f));
    TEST_ASSERT_EQUAL_FLOAT(-50.f, sanitize(-1e6f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fast_exp2_accuracy);
    RUN_TEST(test_slew_time);
    RUN_TEST(test_gate_hysteresis);
    RUN_TEST(test_soft_clip_bounds);
    RUN_TEST(test_sanitize);
    RUN_TEST(test_note_stack);
    RUN_TEST(test_slew_reaches_target_exactly);
    RUN_TEST(test_ladder_matches_daisysp);
    RUN_TEST(test_sanitize_block_matches);
    RUN_TEST(test_ladder_block_matches_process);
    RUN_TEST(test_quantize_matches_old);
    RUN_TEST(test_soft_clip_bounded);
    return UNITY_END();
}
