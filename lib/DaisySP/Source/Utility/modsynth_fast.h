// modsynth: fast math for the per-sample paths on the ESP32-S3 (no FPU divide; libm sinf/powf
// cost hundreds of cycles). Not part of upstream DaisySP. Each helper is within a few float ulps
// of the libm call it replaces on its stated range.
#pragma once
#ifndef DSY_MODSYNTH_FAST_H
#define DSY_MODSYNTH_FAST_H

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

namespace daisysp
{
// rand() for noise sources: newlib's rand() costs ~120 cycles (reentrancy struct and a 64-bit
// multiply). On the device a 32-bit LCG gives the same range [0, RAND_MAX] (a different but
// equally white sequence); natively it stays rand(), so the tests compare sample for sample.
inline int FastRand()
{
#if defined(__XTENSA__)
    static uint32_t s = 22222u; // audio thread only
    s                 = s * 1664525u + 1013904223u;
    return (int)(s >> 1);
#else
    return rand();
#endif
}

// 1/x for x > 0 (no 0, Inf or NaN): recip0.s plus two Newton steps (~1 ulp). Same as
// src/core/dsp_util.h recipPos.
inline float FastRecip(float x)
{
#if defined(__XTENSA__)
    float r;
    asm("recip0.s %0, %1" : "=f"(r) : "f"(x));
    r = r * (2.f - x * r);
    return r * (2.f - x * r);
#else
    return 1.f / x;
#endif
}

// 2^x, relative error ~2e-7 on [-126, 126]. Same as src/core/dsp_util.h fastExp2.
inline float FastExp2(float x)
{
    if(!(x > -126.f))
        x = -126.f;
    if(x > 126.f)
        x = 126.f;
    int i = (int)x;
    if((float)i > x)
        i--;
    const float f = x - (float)i;
    const float p
        = 0.99999984f
          + f
                * (0.69315472f
                   + f
                         * (0.24014658f
                            + f * (0.05583582f + f * (0.00898735f + f * 0.00187537f))));
    const uint32_t bits = (uint32_t)(i + 127) << 23;
    float          scale;
    memcpy(&scale, &bits, sizeof scale);
    return p * scale;
}

// sin(t) for |t| <= pi/2: Taylor to t^11, truncation error < 6e-8.
inline float FastSinHalfPi(float t)
{
    const float t2 = t * t;
    return t
           * (1.f
              + t2
                    * (-1.6666667e-1f
                       + t2
                             * (8.3333333e-3f
                                + t2
                                      * (-1.9841270e-4f
                                         + t2 * (2.7557319e-6f + t2 * -2.5052108e-8f)))));
}

// sin(2 pi phase) for any |phase| < 2^22.
inline float FastSin2Pi(float phase)
{
    int i = (int)phase;
    if((float)i > phase)
        i--;
    float x = phase - (float)i; // [0, 1)
    if(x > 0.5f)
        x -= 1.f; // [-0.5, 0.5]
    if(x > 0.25f)
        x = 0.5f - x;
    else if(x < -0.25f)
        x = -0.5f - x; // [-0.25, 0.25]
    return FastSinHalfPi(x * 6.28318531f);
}

// sqrt(x) for x >= 0: rsqrt0.s plus two Newton steps (libm sqrtf is a call).
inline float FastSqrt(float x)
{
#if defined(__XTENSA__)
    if(!(x > 0.f))
        return 0.f;
    float y;
    asm("rsqrt0.s %0, %1" : "=f"(y) : "f"(x));
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return x * y;
#else
    return x > 0.f ? sqrtf(x) : 0.f;
#endif
}

// x^0.25 for x >= 0 (powf(x, 0.25f) within a few ulps).
inline float FastPowQuarter(float x)
{
    return FastSqrt(FastSqrt(x));
}

} // namespace daisysp
#endif
