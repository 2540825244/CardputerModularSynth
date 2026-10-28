#pragma once
#include <cstdint>
#include "../dsp_util.h"
#include "../module.h"

// Small helpers shared by the v1.2 modules.
namespace mods {

inline const float* inOrNull(const float* const* ins, const bool* c, int i) { return c[i] ? ins[i] : nullptr; }
inline float at(const float* p, int k, float d = 0.f) { return p ? p[k] : d; }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float hzClamp(float hz, float lo = 0.5f, float hi = 18000.f) { return clampf(hz, lo, hi); }

struct Rng {  // LCG, audio-thread safe
    uint32_t s = 0x12345678u;
    uint32_t next() { return s = s * 1664525u + 1013904223u; }
    float uni() { return (float)(next() >> 8) * (1.f / 16777216.f); }  // 0..1
    float bi() { return uni() * 2.f - 1.f; }                            // -1..1
};

// int16 delay segment inside a pool slot (stores volts / 10).
struct Seg {
    int16_t* buf = nullptr;
    int len = 1, w = 0;
    void write(float v) {
        v = v < -10.f ? -10.f : (v > 10.f ? 10.f : v);
        buf[w] = (int16_t)(v * 3276.7f);
        if (++w >= len) w = 0;
    }
    float read(int d) const {  // d in 1..len
        int i = w - d;
        if (i < 0) i += len;
        return (float)buf[i] * (10.f / 32767.f);
    }
    float readFrac(float d) const {
        const int di = (int)d;
        const float f = d - (float)di;
        const float a = read(di < 1 ? 1 : di), b = read(di + 1 > len ? len : di + 1);
        return a + (b - a) * f;
    }
};

}  // namespace mods
