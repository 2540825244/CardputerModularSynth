#include <unity.h>
#include "ui/tilt_filter.h"

void setUp() {}
void tearDown() {}

static const float kS45 = 0.70710678f;

void test_tilt_filter_flat_is_zero() {
    TiltFilter f;
    f.push(0.f, 0.f, 1.f, true, 10.f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.f, f.angles().roll);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.f, f.angles().pitch);
}

void test_tilt_filter_roll_45() {
    TiltFilter f;
    f.push(kS45, 0.f, kS45, true, 10.f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.f, f.angles().roll);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.f, f.angles().pitch);
}

void test_tilt_filter_vertical_is_stable() {
    TiltFilter f;  // held upright (az = 0), rolled 10 degrees
    f.push(0.17365f, 0.98481f, 0.f, true, 10.f);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 10.f, f.angles().roll);
}

void test_tilt_filter_keeps_angle_without_fresh_sample() {
    TiltFilter f;
    f.push(kS45, 0.f, kS45, true, 10.f);
    for (int i = 0; i < 5; i++) f.push(0.f, 0.f, 0.f, false, 10.f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.f, f.angles().roll);
}

void test_tilt_filter_ignores_shake() {
    TiltFilter f;
    f.push(kS45, 0.f, kS45, true, 10.f);
    f.push(3.f, 0.f, 0.f, true, 10.f);
    f.push(0.1f, 0.f, 0.1f, true, 10.f);  // free fall
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.f, f.angles().roll);
}

void test_tilt_filter_smooths() {
    TiltFilter f;
    f.push(0.f, 0.f, 1.f, true, 10.f);
    f.push(kS45, 0.f, kS45, true, 10.f);
    const float r = f.angles().roll;
    TEST_ASSERT_TRUE(r > 0.f && r < 45.f);
    for (int i = 0; i < 30; i++) f.push(kS45, 0.f, kS45, true, 10.f);
    TEST_ASSERT_TRUE(f.angles().roll > 44.f);
}

void test_tilt_filter_invalid_until_sample() {
    TiltFilter f;
    TEST_ASSERT_FALSE(f.valid());
    f.push(0.f, 0.f, 1.f, false, 10.f);
    TEST_ASSERT_FALSE(f.valid());
    f.push(0.f, 0.f, 1.f, true, 10.f);
    TEST_ASSERT_TRUE(f.valid());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tilt_filter_flat_is_zero);
    RUN_TEST(test_tilt_filter_roll_45);
    RUN_TEST(test_tilt_filter_vertical_is_stable);
    RUN_TEST(test_tilt_filter_keeps_angle_without_fresh_sample);
    RUN_TEST(test_tilt_filter_ignores_shake);
    RUN_TEST(test_tilt_filter_smooths);
    RUN_TEST(test_tilt_filter_invalid_until_sample);
    return UNITY_END();
}
