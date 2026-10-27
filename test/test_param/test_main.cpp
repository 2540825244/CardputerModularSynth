#include <unity.h>
#include <cmath>
#include <cstring>
#include "core/param.h"

void setUp() {}
void tearDown() {}

static const ParamSpec kFreq = {"freq", "FRQ", 20.f, 20000.f, 1200.f, Curve::Log, Unit::Hz, nullptr};
static const char* const kWaves[] = {"SIN", "TRI", "SAW", "SQR"};
static const ParamSpec kWave = {"wave", "WAV", 0.f, 3.f, 1.f, Curve::Stepped, Unit::Enum, kWaves};
static const ParamSpec kLin = {"x", "X", 0.f, 1.f, 0.f, Curve::Lin, Unit::None, nullptr};

void test_log_roundtrip() {
    const ParamSpec s = {"f", "F", 20.f, 20000.f, 20.f, Curve::Log, Unit::Hz, nullptr};
    const float ns[] = {0.f, .25f, .5f, 1.f};
    for (float n : ns) TEST_ASSERT_FLOAT_WITHIN(1e-4f, n, paramToNorm(s, paramFromNorm(s, n)));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 632.46f, paramFromNorm(s, .5f));
}

void test_coarse_keys() {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20000.f, paramCoarse(kFreq, 9));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 20.f, paramCoarse(kFreq, 10));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1200.f, paramCoarse(kFreq, 11));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 632.46f, paramCoarse(kFreq, 4));
    float ratio = paramCoarse(kFreq, 5) / paramCoarse(kFreq, 4);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, powf(1000.f, 0.1f), ratio);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.f, paramCoarse(kAmountSpec, 4));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.f, paramCoarse(kAmountSpec, 10));
}

void test_coarse_stepped() {
    TEST_ASSERT_EQUAL_FLOAT(2.f, paramCoarse(kWave, 2));
    TEST_ASSERT_EQUAL_FLOAT(3.f, paramCoarse(kWave, 7));
    TEST_ASSERT_EQUAL_FLOAT(1.f, paramCoarse(kWave, 11));
    TEST_ASSERT_EQUAL_FLOAT(0.f, paramCoarse(kWave, 10));
}

void test_fine_steps() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f + 1.f / 128.f, paramFine(kLin, 0.5f, 1));
    TEST_ASSERT_EQUAL_FLOAT(3.f, paramFine(kWave, 3.f, 1));
    TEST_ASSERT_EQUAL_FLOAT(2.f, paramFine(kWave, 3.f, -1));
}

void test_clamp_out_of_range() {
    TEST_ASSERT_EQUAL_FLOAT(20000.f, paramClamp(kFreq, 1e9f));
    TEST_ASSERT_EQUAL_FLOAT(1200.f, paramClamp(kFreq, NAN));
    TEST_ASSERT_EQUAL_FLOAT(3.f, paramClamp(kWave, 2.6f));
}

void test_format() {
    char b[16];
    paramFormat(kFreq, 1200.f, b, sizeof b, true);
    TEST_ASSERT_EQUAL_STRING("1.20KHZ", b);
    paramFormat(kFreq, 1200.f, b, sizeof b, false);
    TEST_ASSERT_EQUAL_STRING("1.2K", b);
    const ParamSpec pct = {"m", "M", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr};
    paramFormat(pct, 0.4f, b, sizeof b, true);
    TEST_ASSERT_EQUAL_STRING("40%", b);
    paramFormat(kAmountSpec, -0.35f, b, sizeof b, true);
    TEST_ASSERT_EQUAL_STRING("-35", b);
    paramFormat(kAmountSpec, 1.f, b, sizeof b, true);
    TEST_ASSERT_EQUAL_STRING("+100", b);
    paramFormat(kWave, 2.f, b, sizeof b, true);
    TEST_ASSERT_EQUAL_STRING("SAW", b);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_log_roundtrip);
    RUN_TEST(test_coarse_keys);
    RUN_TEST(test_coarse_stepped);
    RUN_TEST(test_fine_steps);
    RUN_TEST(test_clamp_out_of_range);
    RUN_TEST(test_format);
    return UNITY_END();
}
