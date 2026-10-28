#include "anim.h"
#include <cmath>

float ease(float from, float to, float dtMs, float tauMs) {
    if (tauMs <= 0.f || dtMs <= 0.f) return dtMs <= 0.f ? from : to;
    const float k = 1.f - expf(-dtMs / tauMs);
    const float v = from + (to - from) * k;
    return fabsf(to - v) < 1e-3f ? to : v;
}

float zoomT(uint32_t since, uint32_t now) {
    const int32_t d = (int32_t)(now - since);
    if (d <= 0) return 0.f;
    if (d >= (int32_t)kZoomMs) return 1.f;
    const float t = (float)d / (float)kZoomMs;
    return t * t * (3.f - 2.f * t);
}

float flashT(uint32_t since, uint32_t now, uint32_t dur) {
    const int32_t d = (int32_t)(now - since);
    if (d < 0 || d >= (int32_t)dur) return 0.f;
    return 1.f - (float)d / (float)dur;
}
