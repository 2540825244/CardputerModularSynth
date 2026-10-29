#pragma once
#include <cstdint>
#include <cstring>
#include "units.h"

// Per-sample helpers live here, inline: called across translation units they cost a full call
// each (bench: sanitize 50, Slew 37 cycles/sample out of line on the ESP32-S3).

// 1/x for x > 0 (no 0, Inf or NaN). The S3 FPU has no divide: a float '/' is a ~60-cycle ROM
// call; recip0.s plus two Newton steps is ~1 ulp.
inline float recipPos(float x) {
#if defined(__XTENSA__)
    float r;
    asm("recip0.s %0, %1" : "=f"(r) : "f"(x));
    r = r * (2.f - x * r);
    return r * (2.f - x * r);
#else
    return 1.f / x;
#endif
}

inline float fastExp2(float x) {  // |rel err| < 1e-4 on [-12, 12], no libm
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
inline float voltsToHz(float v) { return kC4Hz * fastExp2(v); }

struct Slew {  // one-pole lowpass; ms = 0 -> passthrough
    float coef = 1.f, y = 0.f;
    void setTimeMs(float ms, float fs);
    float process(float x) {
        const float d = x - y;
        // Snap the last micro-step: a one-pole otherwise ends in a subnormal tail that never reaches x.
        y = d < 1e-6f && d > -1e-6f ? x : y + coef * d;
        return y;
    }
};

struct GateDetector {  // kGateOn / kGateOff hysteresis; rose is true for one call
    bool high = false;
    bool rose = false;
    bool process(float v) {
        rose = false;
        if (!high && v > kGateOn) {
            high = true;
            rose = true;
        } else if (high && v < kGateOff) {
            high = false;
        }
        return high;
    }
};

// smooth, odd, |out| <= 1, ~x for small x: rational tanh approximation, exactly +-1 for |x| >= 3
inline float softClip(float x) {
    if (x >= 3.f) return 1.f;
    if (x <= -3.f) return -1.f;
    const float x2 = x * x;
    const float y = x * (27.f + x2) * recipPos(27.f + 9.f * x2);
    return y > 1.f ? 1.f : (y < -1.f ? -1.f : y);
}

inline float sanitize(float x) {  // NaN/Inf -> 0, else clamp +-50 V
    if (!(x <= 3.0e38f && x >= -3.0e38f)) return 0.f;  // NaN fails both
    return x > 50.f ? 50.f : (x < -50.f ? -50.f : x);
}
