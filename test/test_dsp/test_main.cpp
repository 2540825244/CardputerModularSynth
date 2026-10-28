#include <unity.h>
#include <cmath>
#include "core/dsp_util.h"
#include "core/note_stack.h"

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

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fast_exp2_accuracy);
    RUN_TEST(test_slew_time);
    RUN_TEST(test_gate_hysteresis);
    RUN_TEST(test_soft_clip_bounds);
    RUN_TEST(test_sanitize);
    RUN_TEST(test_note_stack);
    RUN_TEST(test_slew_reaches_target_exactly);
    return UNITY_END();
}
