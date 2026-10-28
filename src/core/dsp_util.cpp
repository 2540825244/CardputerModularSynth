#include "dsp_util.h"
#include <cmath>
#include <cstdint>
#include <cstring>

float fastExp2(float x) {
    if (!(x > -126.f)) x = -126.f;  // also catches NaN
    if (x > 126.f) x = 126.f;
    int i = (int)x;
    if ((float)i > x) i--;  // floor without libm
    const float f = x - (float)i;
    // least-squares fit of 2^f on [0,1), relative error ~2e-7
    const float p = 0.99999984f + f * (0.69315472f + f * (0.24014658f + f * (0.05583582f +
                    f * (0.00898735f + f * 0.00187537f))));
    const uint32_t bits = (uint32_t)(i + 127) << 23;
    float scale;
    memcpy(&scale, &bits, sizeof scale);
    return p * scale;
}

void Slew::setTimeMs(float ms, float fs) {
    if (ms <= 0.f) {
        coef = 1.f;
        return;
    }
    coef = 1.f - expf(-1.f / (ms * 0.001f * fs));  // UI thread only
}

float Slew::process(float x) {
    const float d = x - y;
    // Snap the last micro-step: a one-pole otherwise ends in a subnormal tail that never reaches x.
    y = d < 1e-6f && d > -1e-6f ? x : y + coef * d;
    return y;
}

bool GateDetector::process(float v) {
    rose = false;
    if (!high && v > kGateOn) {
        high = true;
        rose = true;
    } else if (high && v < kGateOff) {
        high = false;
    }
    return high;
}

float softClip(float x) {
    // rational tanh approximation, exactly +-1 for |x| >= 3
    if (x >= 3.f) return 1.f;
    if (x <= -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

float sanitize(float x) {
    if (!(x == x) || x > 3.0e38f || x < -3.0e38f) return 0.f;  // NaN / Inf
    if (x > 50.f) return 50.f;
    if (x < -50.f) return -50.f;
    return x;
}
