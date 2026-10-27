#include <unity.h>
#include "core/units.h"
#include "Synthesis/oscillator.h"

void setUp() {}
void tearDown() {}

void test_oscillator_in_range() {
    daisysp::Oscillator osc;
    osc.Init(kSampleRate);
    osc.SetFreq(kC4Hz);
    osc.SetAmp(1.f);
    for (int i = 0; i < 1000; i++) {
        float v = osc.Process();
        TEST_ASSERT_TRUE(v >= -1.f && v <= 1.f);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_oscillator_in_range);
    return UNITY_END();
}
