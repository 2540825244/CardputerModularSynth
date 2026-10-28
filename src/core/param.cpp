#include "param.h"
#include <cmath>
#include <cstdio>

const ParamSpec kAmountSpec = {"amount", "AMT", -1.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr};

static float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

float paramClamp(const ParamSpec& s, float v) {
    if (std::isnan(v)) return s.def;
    if (v < s.min) v = s.min;
    if (v > s.max) v = s.max;
    if (s.curve == Curve::Stepped) v = roundf(v);
    return v;
}

float paramToNorm(const ParamSpec& s, float v) {
    v = paramClamp(s, v);
    const float span = s.max - s.min;
    if (span <= 0.f) return 0.f;
    float n;
    switch (s.curve) {
        case Curve::Log: n = logf(v / s.min) / logf(s.max / s.min); break;
        case Curve::Sqr: n = sqrtf((v - s.min) / span); break;
        default: n = (v - s.min) / span; break;
    }
    return clamp01(n);
}

float paramFromNorm(const ParamSpec& s, float n) {
    n = std::isnan(n) ? 0.f : clamp01(n);
    float v;
    switch (s.curve) {
        case Curve::Log: v = s.min * powf(s.max / s.min, n); break;
        case Curve::Sqr: v = s.min + (s.max - s.min) * n * n; break;
        case Curve::Stepped: v = roundf(s.min + (s.max - s.min) * n); break;
        default: v = s.min + (s.max - s.min) * n; break;
    }
    return paramClamp(s, v);
}

float paramCoarse(const ParamSpec& s, int col) {
    if (col < 0) col = 0;
    if (col > 11) col = 11;
    if (col == 11) return s.def;
    if (col == 10) return s.min;
    if (s.curve == Curve::Stepped && s.max - s.min < 10.f) return paramClamp(s, s.min + (float)col);
    return paramFromNorm(s, col == 9 ? 1.f : (float)(col + 1) / 10.f);
}

float paramFine(const ParamSpec& s, float v, int dir) {
    const float d = dir > 0 ? 1.f : (dir < 0 ? -1.f : 0.f);
    if (s.curve == Curve::Stepped) return paramClamp(s, v + d);
    return paramFromNorm(s, paramToNorm(s, v) + d / 128.f);
}

void paramFormat(const ParamSpec& s, float v, char* out, int len, bool longForm) {
    v = paramClamp(s, v);
    switch (s.unit) {
        case Unit::Hz:
            if (v >= 1000.f) snprintf(out, len, longForm ? "%.2fKHZ" : "%.1fK", (double)(v / 1000.f));
            else snprintf(out, len, longForm ? "%.0fHZ" : "%.0f", (double)v);
            break;
        case Unit::Ms:
            if (v >= 1000.f) snprintf(out, len, longForm ? "%.2fS" : "%.1fS", (double)(v / 1000.f));
            else snprintf(out, len, longForm ? "%.0fMS" : "%.0f", (double)v);
            break;
        case Unit::Percent: {
            const float p = roundf(v * 100.f) + 0.f;  // + 0 turns -0 into 0
            if (s.min < 0.f) snprintf(out, len, "%+.0f", (double)p);
            else snprintf(out, len, "%.0f%%", (double)p);
            break;
        }
        case Unit::Semi:
        case Unit::Cents:
        case Unit::Oct: snprintf(out, len, "%+.0f", (double)roundf(v)); break;
        case Unit::Enum: {
            const int i = (int)(v - s.min);
            snprintf(out, len, "%s", s.enumNames ? s.enumNames[i] : "?");
            break;
        }
        case Unit::Volts: snprintf(out, len, longForm ? "%+.2fV" : "%+.1f", (double)(v + 0.f)); break;
        case Unit::Bpm: snprintf(out, len, longForm ? "%.0fBPM" : "%.0f", (double)v); break;
        case Unit::Deg: snprintf(out, len, longForm ? "%.0fDEG" : "%.0f", (double)v); break;
        default: snprintf(out, len, "%.2f", (double)v); break;
    }
}
