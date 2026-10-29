// Pictures: filters, shapers, effects.
#include <cstdio>
#include <initializer_list>
#include "pictures_internal.h"

namespace pic {
namespace {

// ---- VCF: DaisySP Svf response, Q = 1 / (2 (1 - res^0.25)) ----
float svfDb(float f, float fc, float res, int mode) {
    float q = 1.f / (2.f * (1.f - powf(clampf(res, 0.f, 0.999f), 0.25f)));
    q = q > 20.f ? 20.f : q;
    const float r = f / fc, r2 = r * r, d = 1.f - r2;
    float mag = 1.f / sqrtf(d * d + (r / q) * (r / q));
    if (mode == 1) mag *= r2;
    else if (mode == 2) mag *= r / q;
    else if (mode == 3) mag *= fabsf(d);
    return 20.f * log10f(mag + 1e-6f);
}
inline float logF(float u) { return 20.f * powf(1000.f, u); }      // u 0..1 -> 20 Hz..20 kHz
inline float uOfF(float f) { return log10f(clampf(f, 20.f, 20000.f) / 20.f) / 3.f; }
inline float dbUnit(float db) { return 1.f - (12.f - db) / 60.f; }  // +12 dB top, -48 dB bottom

void vcf(const PicCtx& c) {
    const Box b = body(c);
    float fc = mon(c, 0, 0.f);
    if (fc < 20.f) fc = par(c, 0);
    const float res = live(c) ? mon(c, 1, par(c, 1)) : par(c, 1);
    const int mode = ipar(c, 2);
    if (!c.small)
        for (float f : {100.f, 1000.f, 10000.f}) dashV(c.g, xUnit(b, uOfF(f)), b.y, b.h, PBorder, 1, 2);
    const ParamSpec* sp = c.m.type->params;
    if (!c.small && (fabsf(par(c, 0) - sp[0].def) > 1.f || fabsf(par(c, 1) - sp[1].def) > 0.01f))
        traceUnit(c.g, b, [&](float u) { return dbUnit(svfDb(logF(u), sp[0].def, sp[1].def, mode)); }, PFaint, false, 2);
    const bool bold = c.sel >= 0 && c.sel <= 2;
    traceUnit(c.g, b, [&](float u) { return dbUnit(svfDb(logF(u), fc, res, mode)); }, PAccent, bold);
    if (c.small) return;
    dashV(c.g, xUnit(b, uOfF(fc)), b.y, b.h, isSel(c, 0) ? PAccent : PDim);
    static const char* const kModes[] = {"LP 12DB", "HP 12DB", "BP 12DB", "NOTCH"};
    char hz[12];
    paramFormat(c.m.type->params[0], fc, hz, sizeof hz, true);
    captions(c, kModes[mode < 0 ? 0 : (mode > 3 ? 3 : mode)], ink(c, 2) == PAccent ? PAccent : PDim, hz,
             ink(c, 0));
    char q[16];
    snprintf(q, sizeof q, "RES %d%%", (int)lroundf(res * 100.f));
    label(c, q, b.x + 1, b.y + b.h - 6, isSel(c, 1) ? PAccent : PDim);
    if (par(c, 3) > 0.005f || isSel(c, 3)) {
        char d[16];
        snprintf(d, sizeof d, "DRV %d%%", (int)lroundf(par(c, 3) * 100.f));
        label(c, d, b.x + b.w - 1, b.y + b.h - 6, isSel(c, 3) ? PAccent : PDim, Align::Right);
    }
}
bool vcfGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    const int mode = (int)lroundf(m.params[2]);
    traceUnit(g, b, [&](float u) { return dbUnit(svfDb(logF(u), m.params[0], m.params[1], mode)) * 1.15f - 0.1f; }, col);
    return true;
}


// ---- TRM: a fast carrier under the tremolo's gain (2 LFO cycles of WAV at DEP) ----
void trm(const PicCtx& c) {
    const int w = ipar(c, 2);
    const float dep = clampf(par(c, 1), 0.f, 1.f);
    auto lfoU = [&](float t) {  // 0..1
        const float p = t - floorf(t);
        if (w == 1) return p < 0.5f ? 2.f * p : 2.f - 2.f * p;
        if (w == 2) return p < 0.5f ? 1.f : 0.f;
        return 0.5f + 0.5f * sinf(6.2831853f * p);
    };
    auto gain = [&](float u) { return 1.f - dep * lfoU(u * 2.f); };
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 3);
    if (!c.small) {
        trace(c.g, b, [&](float u) { return gain(u) * 0.95f; }, isSel(c, 1) ? PAccent : PDim);
        trace(c.g, b, [&](float u) { return -gain(u) * 0.95f; }, isSel(c, 1) ? PAccent : PDim);
    }
    trace(c.g, b, [&](float u) { return sinf(6.2831853f * u * 11.f) * gain(u) * 0.95f; }, c.sel < 0 || c.sel == 2 ? PAccent : PText);
    if (c.small) return;
    char r[16];
    fmt(c, 0, r, sizeof r, true);
    static const char* const kW[] = {"SIN", "TRI", "SQR"};
    captions(c, kW[w < 0 ? 0 : (w > 2 ? 2 : w)], isSel(c, 2) ? PAccent : PDim, r, isSel(c, 0) ? PAccent : PText);
}

}  // namespace

const Entry kProcPics[] = {
    {"vcf", vcf, vcfGlyph}, {"trm", trm, nullptr},
};
const int kNumProcPics = sizeof kProcPics / sizeof kProcPics[0];

}  // namespace pic
