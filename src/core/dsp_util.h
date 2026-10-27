#pragma once
#include "units.h"

float fastExp2(float x);  // |rel err| < 1e-4 on [-12, 12], no libm
inline float voltsToHz(float v) { return kC4Hz * fastExp2(v); }

struct Slew {  // one-pole lowpass; ms = 0 -> passthrough
    float coef = 1.f, y = 0.f;
    void setTimeMs(float ms, float fs);
    float process(float x);
};

struct GateDetector {  // kGateOn / kGateOff hysteresis; rose is true for one call
    bool high = false;
    bool rose = false;
    bool process(float v);
};

float softClip(float x);  // smooth, odd, |out| <= 1, ~x for small x
float sanitize(float x);  // NaN/Inf -> 0, else clamp +-50 V
