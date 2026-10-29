// Pictures: filters, shapers, effects.
#include <cstdio>
#include <complex>
#include <initializer_list>
#include "Effects/overdrive.h"
#include "Effects/wavefolder.h"
#include "core/modules/common.h"
#include "core/modules/v14.h"
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
        if (w == 2) return p < 0.5f ? 0.f : 1.f;
        return 0.5f - 0.5f * sinf(6.2831853f * p);
    };
    auto gain = [&](float u) { return 1.f - dep * lfoU(u * 2.f); };  // DaisySP: SIN rises, SQR starts at 1
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 3);
    if (!c.small) {
        trace(c.g, b, [&](float u) { return gain(u) * 0.95f; }, isSel(c, 1) ? PAccent : PDim);
        trace(c.g, b, [&](float u) { return -gain(u) * 0.95f; }, isSel(c, 1) ? PAccent : PDim);
    }
    trace(c.g, b, [&](float u) { return sinf(6.2831853f * u * 11.f) * gain(u) * 0.95f; }, c.sel < 0 || c.sel == 2 ? PAccent : PText);
    if (c.small) return;
    char r[16];
    hzText(par(c, 0), r, sizeof r);
    static const char* const kW[] = {"SIN", "TRI", "SQR"};
    captions(c, kW[w < 0 ? 0 : (w > 2 ? 2 : w)], isSel(c, 2) ? PAccent : PDim, r, isSel(c, 0) ? PAccent : PText);
}

// ---- LPG: after a strike, the vactrol's level (solid) and its cutoff (dashed, by COL) ----
void lpg(const PicCtx& c) {
    const float decMs = par(c, 0), color = par(c, 1), level = par(c, 2);
    const float winMs = clampf(4.f * decMs, 60.f, 12000.f);
    auto env = [&](float u) { return level + (1.f - level) * expf(-u * winMs / decMs); };
    auto cut = [&](float u) {
        const float hz = 40.f * powf(400.f, env(u) * color + (1.f - color));
        return log10f(clampf(hz, 20.f, 18000.f) / 20.f) / log10f(900.f);
    };
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 3);
    if (!c.small) dashH(c.g, b.x, yUnit(b, level), b.w, isSel(c, 2) ? PAccent : PBorder);
    traceUnit(c.g, b, cut, isSel(c, 1) ? PAccent : PDim, false, c.small ? 0 : 2);
    traceUnit(c.g, b, env, c.sel < 0 || c.sel == 0 ? PAccent : PText, isSel(c, 0));
    if (c.small) return;
    if (live(c)) {
        const int y = yUnit(b, mon(c, 0, 0.f));
        c.g.fillRect(c.x + c.w - 5, y - 1, 4, 3, PText);
    }
    char t[12];
    if (winMs >= 1000.f) snprintf(t, sizeof t, "%.1fS", (double)(winMs / 1000.f));
    else snprintf(t, sizeof t, "%dMS", (int)lroundf(winMs));
    captions(c, "LEVEL  - - CUTOFF", PDim, t, PDim);
}

// ---- transfer curves: the shaper's out against in (left), a sine through it (right) ----
template <class F>
void transfer(const PicCtx& c, F shape, uint8_t col, bool bold, const char* left, const char* right, uint8_t rcol) {
    const Box b = body(c);
    if (c.small) {
        trace(c.g, b, [&](float u) { return shape(sinf(6.2831853f * u * 1.5f)); }, col);
        return;
    }
    const int sq = b.h < b.w * 45 / 100 ? b.h : b.w * 45 / 100;
    const Box l = {b.x, b.y + (b.h - sq) / 2, sq, sq};
    c.g.rect(l.x, l.y, l.w, l.h, PBorder);
    dashH(c.g, l.x, l.y + l.h / 2, l.w, PBorder, 1, 2);
    dashV(c.g, l.x + l.w / 2, l.y, l.h, PBorder, 1, 2);
    const Box li = inset(l, 1);
    trace(c.g, li, [&](float u) { return shape(u * 2.f - 1.f); }, col, bold);
    const Box r = {l.x + l.w + 6, b.y + 4, b.x + b.w - (l.x + l.w + 6), b.h - 8};
    trace(c.g, r, [&](float u) { return sinf(6.2831853f * u * 1.5f) * 0.6f; }, PFaint, false, 2);
    trace(c.g, r, [&](float u) { return shape(sinf(6.2831853f * u * 1.5f)); }, col, false);
    captions(c, left, PDim, right, rcol);
}

// FLD: DaisySP's Wavefolder at GAN / OFS (the module feeds it in x 0.2: a 5 V sine -> 1)
void fld(const PicCtx& c) {
    daisysp::Wavefolder f;
    f.Init();
    f.SetGain(clampf(par(c, 0), 0.5f, 40.f));
    f.SetOffset(par(c, 1));
    auto shape = [&](float x) { return clampf(f.Process(x), -1.f, 1.f) * 0.95f; };
    int folds = 0;
    float prev = shape(-1.f), lastD = 0.f;
    for (int i = 1; i <= 200; i++) {  // turning points of the curve
        const float v = shape(-1.f + 2.f * (float)i / 200.f), d = v - prev;
        if (d * lastD < 0.f) folds++;
        if (d != 0.f) lastD = d;
        prev = v;
    }
    char r[16];
    snprintf(r, sizeof r, "%d FOLDS", folds);
    transfer(c, shape, PAccent, c.sel == 0 || c.sel == 1, "IN > OUT", r, c.sel >= 0 ? PAccent : PText);
}
// OVD: DaisySP's Overdrive at DRV (TON and MIX filter / blend after it)
void ovd(const PicCtx& c) {
    daisysp::Overdrive d;
    d.Init();
    d.SetDrive(clampf(par(c, 0), 0.f, 1.f));
    const float mix = clampf(par(c, 2), 0.f, 1.f);
    auto shape = [&](float x) { return clampf(x * 0.95f * (1.f - mix) + d.Process(x) * mix, -1.f, 1.f) * 0.95f; };
    char r[16];
    snprintf(r, sizeof r, "DRIVE %d%%", (int)lroundf(par(c, 0) * 100.f));
    transfer(c, shape, PAccent, isSel(c, 0), "IN > OUT", r, isSel(c, 0) ? PAccent : PText);
}
// CRU: a sine held at RAT (against a 440 Hz sine) and rounded to BIT levels
void cru(const PicCtx& c) {
    const int bits = ipar(c, 0);
    const float steps = (float)(1 << (bits < 1 ? 0 : (bits > 16 ? 15 : bits - 1)));
    const float rate = clampf(par(c, 1), 50.f, 48000.f), mix = clampf(par(c, 2), 0.f, 1.f);
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 3);
    const float cycles = 2.f, f0 = 440.f;  // 2 cycles of 440 Hz across the box
    const float holdU = f0 / rate / cycles;  // one hold, as a fraction of the box
    auto in = [&](float u) { return sinf(6.2831853f * u * cycles) * 0.9f; };
    auto held = [&](float u) {
        const float uh = holdU > 0.f ? floorf(u / holdU) * holdU : u;
        const float v = roundf(in(uh) * steps) / steps;
        return in(u) * (1.f - mix) + v * mix;
    };
    if (!c.small) trace(c.g, b, in, PFaint, false, 2);
    trace(c.g, b, held, c.sel < 0 || c.sel <= 1 ? PAccent : PText, c.sel == 0 || c.sel == 1);
    if (c.small) return;
    char l[16], r[16];
    snprintf(l, sizeof l, "%d BITS", bits);
    fmt(c, 1, r, sizeof r, true);
    captions(c, l, isSel(c, 0) ? PAccent : PDim, r, isSel(c, 1) ? PAccent : PText);
}
// ATN: out = AMT x in + OFS, in a +-5 V box
void atn(const PicCtx& c) {
    const float amt = par(c, 0), ofs = par(c, 1) / 5.f;
    auto shape = [&](float x) { return clampf(amt * x + ofs, -1.f, 1.f) * 0.95f; };
    char r[16];
    snprintf(r, sizeof r, "%+d%% %+.1fV", (int)lroundf(amt * 100.f), (double)par(c, 1));
    transfer(c, shape, PAccent, c.sel >= 0, "IN > OUT", r, c.sel >= 0 ? PAccent : PText);
}
// DYN: the static curve (in dB -> out dB, THR knee, RAT slope, MKP lift); live gain reduction
void dyn(const PicCtx& c) {
    const float th = par(c, 0), slope = 1.f - 1.f / clampf(par(c, 1), 1.f, 20.f), mk = par(c, 4);
    const Box b = body(c);
    const int sq = c.small ? b.h : (b.h < b.w * 70 / 100 ? b.h : b.w * 70 / 100);
    const Box l = c.small ? b : Box{b.x, b.y + (b.h - sq) / 2, sq, sq};
    auto curve = [&](float u) {  // u: in -48..0 dB; out -48..+24 drawn -48..0 (clamped)
        const float in = -48.f + 48.f * u, over = in - th;
        const float out = in - (over > 0.f ? over * slope : 0.f) + mk;
        return clampf((out + 48.f) / 48.f, 0.f, 1.f);
    };
    if (!c.small) {
        c.g.rect(l.x, l.y, l.w, l.h, PBorder);
        trace(c.g, inset(l, 1), [&](float u) { return u * 2.f - 1.f; }, PFaint, false, 2);
        const int tx = xUnit(inset(l, 1), (th + 48.f) / 48.f);
        dashV(c.g, tx, l.y, l.h, isSel(c, 0) ? PAccent : PDim);
    }
    traceUnit(c.g, c.small ? l : inset(l, 1), curve, c.sel < 0 || c.sel == 0 || c.sel == 1 || c.sel == 4 ? PAccent : PText,
              c.sel == 1 || c.sel == 4);
    if (c.small) return;
    // the gain-reduction meter: live (monitor 0 = dB of reduction), 0..24 dB down from the top
    const Box m = {l.x + l.w + 6, l.y, 6, l.h};
    c.g.rect(m.x, m.y, m.w, m.h, PBorder);
    const float gr = clampf(mon(c, 0, 0.f), 0.f, 24.f);
    const int gh = (int)lroundf(gr / 24.f * (m.h - 2));
    if (gh > 0) c.g.fillRect(m.x + 1, m.y + 1, m.w - 2, gh, PAccent);
    label(c, "GR", m.x + m.w + 2, m.y, PDim);
    char g[12];
    snprintf(g, sizeof g, "-%.0fDB", (double)gr);
    label(c, g, m.x + m.w + 2, m.y + 8, PText);
    char l2[24], r[16];
    snprintf(l2, sizeof l2, "THR %.0fDB", (double)th);
    snprintf(r, sizeof r, "%.1f:1", (double)par(c, 1));
    captions(c, l2, isSel(c, 0) ? PAccent : PDim, r, isSel(c, 1) ? PAccent : PText);
}
// RING: a fixed A (3 cycles) and B (4.5 cycles) through the four outs
void ring(const PicCtx& c) {
    auto a = [](float u) { return sinf(6.2831853f * u * 3.f) * 0.9f; };
    auto bb = [](float u) { return sinf(6.2831853f * u * 4.5f) * 0.9f; };
    const Box b = body(c);
    if (c.small) {
        trace(c.g, b, [&](float u) { return a(u) * bb(u); }, PAccent);
        return;
    }
    static const char* const kN[] = {"MUL", "SUM", "MAX", "MIN"};
    for (int k = 0; k < 4; k++) {
        const Box r = {b.x + 14, b.y + k * b.h / 4 + 1, b.w - 14, b.h / 4 - 2};
        c.g.text(kN[k], b.x, r.y + r.h / 2 - 2, PDim, Align::Left);
        trace(c.g, r, [&](float u) {
            const float x = a(u), y = bb(u);
            return k == 0 ? x * y : (k == 1 ? (x + y) * 0.5f : (k == 2 ? (x > y ? x : y) : (x < y ? x : y)));
        }, k == 0 ? PAccent : PText);
    }
}
// XFD: A and B bars at their gains, the POS marker on the slide
void xfd(const PicCtx& c) {
    const float pos = clampf(par(c, 0), 0.f, 1.f);
    const Box b = body(c);
    const Box track = c.small ? band(b, 0.8f, 1.f) : band(b, 0.82f, 0.92f);
    c.g.fillRect(track.x, track.y + track.h / 2, track.w, 1, PBorder);
    const Box slide = insetXY(track, 2, 0);  // the 5 px knob stays inside at either end
    const int px = xUnit(slide, pos);
    c.g.fillRect(px - 2, track.y, 5, track.h, c.sel <= 0 ? PAccent : PText);
    const Box bars = c.small ? band(b, 0.f, 0.7f) : band(b, 0.05f, 0.75f);
    const int bw = bars.w / 5;
    const int ha = (int)lroundf((1.f - pos) * (bars.h - 1)), hb = (int)lroundf(pos * (bars.h - 1));
    c.g.fillRect(bars.x + bw, bars.y + bars.h - ha, bw, ha, PText);
    c.g.fillRect(bars.x + 3 * bw, bars.y + bars.h - hb, bw, hb, PText);
    if (c.small) return;
    label(c, "A", bars.x + bw + bw / 2, bars.y + bars.h + 1, PDim, Align::Center);
    label(c, "B", bars.x + 3 * bw + bw / 2, bars.y + bars.h + 1, PDim, Align::Center);
    char l[16];
    snprintf(l, sizeof l, "%d / %d", (int)lroundf((1.f - pos) * 100.f), (int)lroundf(pos * 100.f));
    captions(c, "A / B", PDim, l, c.sel <= 0 ? PAccent : PText);
}

// ---- LAD: the ladder's 6 modes, G = 1 / (1 + j f/fc) per pole, k = 4 res around four ----
float ladderDb(float f, float fc, float res, int mode) {
    const std::complex<float> G = 1.f / std::complex<float>(1.f, f / fc);
    const std::complex<float> G2 = G * G, G4 = G2 * G2, one(1.f, 0.f), D = one + 4.f * clampf(res, 0.f, 0.97f) * G4;
    std::complex<float> h;
    switch (mode) {
        case 1: h = G2 / D; break;                                   // LP12
        case 2: h = 4.f * G2 * (one - G) * (one - G) / D; break;     // BP24
        case 3: h = 2.f * G * (one - G) / D; break;                  // BP12
        case 4: h = (one - G) * (one - G) * (one - G) * (one - G) / D; break;  // HP24
        case 5: h = (one - G) * (one - G) / D; break;                // HP12
        default: h = G4 / D; break;                                  // LP24
    }
    return 20.f * log10f(std::abs(h) + 1e-6f);
}
void lad(const PicCtx& c) {
    const Box b = body(c);
    float fc = mon(c, 0, 0.f);
    if (fc < 20.f) fc = par(c, 0);
    const float res = par(c, 1);
    const int mode = ipar(c, 3);
    if (!c.small)
        for (float f : {100.f, 1000.f, 10000.f}) dashV(c.g, xUnit(b, uOfF(f)), b.y, b.h, PBorder, 1, 2);
    traceUnit(c.g, b, [&](float u) { return dbUnit(ladderDb(logF(u), fc, res, mode)); }, PAccent, c.sel == 0 || c.sel == 1 || c.sel == 3);
    if (c.small) return;
    dashV(c.g, xUnit(b, uOfF(fc)), b.y, b.h, isSel(c, 0) ? PAccent : PDim);
    char hz[12], q[16];
    paramFormat(c.m.type->params[0], fc, hz, sizeof hz, true);
    captions(c, enumName(c, 3), isSel(c, 3) ? PAccent : PDim, hz, ink(c, 0));
    snprintf(q, sizeof q, "RES %d%%", (int)lroundf(res * 100.f));
    label(c, q, b.x + 1, b.y + b.h - 6, isSel(c, 1) ? PAccent : PDim);
    if (par(c, 2) > 0.005f) {
        char d[16];
        snprintf(d, sizeof d, "DRV %d%%", (int)lroundf(par(c, 2) * 100.f));
        label(c, d, b.x + b.w - 1, b.y + b.h - 6, isSel(c, 2) ? PAccent : PDim, Align::Right);
    }
}
bool ladGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    const int mode = (int)lroundf(m.params[3]);
    traceUnit(g, b, [&](float u) { return dbUnit(ladderDb(logF(u), m.params[0], m.params[1], mode)) * 1.15f - 0.1f; }, col);
    return true;
}

// ---- AWH: the wah's band-pass at WAH, its sweep range faint ----
float bpDb(float f, float fc, float q) {
    const float r = f / fc, d = 1.f - r * r;
    return 20.f * log10f((r / q) / sqrtf(d * d + (r / q) * (r / q)) + 1e-6f);
}
void awh(const PicCtx& c) {
    const Box b = body(c);
    auto fAt = [](float w) { return 300.f * powf(10.f, w * 1.2f); };  // 300 Hz .. 4.8 kHz
    if (!c.small) {
        traceUnit(c.g, b, [&](float u) { return dbUnit(bpDb(logF(u), fAt(0.f), 4.f)); }, PFaint, false, 2);
        traceUnit(c.g, b, [&](float u) { return dbUnit(bpDb(logF(u), fAt(1.f), 4.f)); }, PFaint, false, 2);
    }
    const float w = clampf(par(c, 0), 0.f, 1.f);
    traceUnit(c.g, b, [&](float u) { return dbUnit(bpDb(logF(u), fAt(w), 4.f)); }, c.sel <= 0 ? PAccent : PText, isSel(c, 0));
    if (c.small) return;
    char l[16];
    snprintf(l, sizeof l, "WAH %d%%", (int)lroundf(w * 100.f));
    captions(c, "SWEEP", PDim, l, isSel(c, 0) ? PAccent : PText);
}

// ---- PHA: 6 all-pass stages mixed with the dry: notches at FRQ, the DEP sweep faint ----
float phaserDb(float f, float fc, float fb) {
    const float phi = -12.f * atanf(f / fc);  // 6 stages of -2 atan
    const std::complex<float> ap = std::polar(1.f, phi), one(1.f, 0.f);
    const std::complex<float> wet = ap / (one - clampf(fb, 0.f, 0.9f) * ap);
    return 20.f * log10f(std::abs(0.5f * (one + wet)) + 1e-6f);
}
void pha(const PicCtx& c) {
    const Box b = body(c);
    const float fc = par(c, 2), dep = clampf(par(c, 1), 0.f, 1.f), fb = par(c, 3);
    if (!c.small) {
        traceUnit(c.g, b, [&](float u) { return dbUnit(phaserDb(logF(u), fc * (1.f - 0.8f * dep), fb)); }, PFaint, false, 2);
        traceUnit(c.g, b, [&](float u) { return dbUnit(phaserDb(logF(u), fc * (1.f + 2.f * dep), fb)); }, PFaint, false, 2);
    }
    traceUnit(c.g, b, [&](float u) { return dbUnit(phaserDb(logF(u), fc, fb)); }, PAccent, c.sel == 2 || c.sel == 3);
    if (c.small) return;
    char hz[12], r[16];
    hzText(par(c, 2), hz, sizeof hz);
    hzText(par(c, 0), r, sizeof r);
    captions(c, hz, isSel(c, 2) ? PAccent : PDim, r, isSel(c, 0) ? PAccent : PText);
}
// ---- FLG: the comb of the swept delay (|1 / (1 - g e^-jwT)| with the dry), centre and ends ----
float combDb(float f, float tMs, float fb) {
    const std::complex<float> z = std::polar(1.f, -6.2831853f * f * tMs * 0.001f), one(1.f, 0.f);
    return 20.f * log10f(std::abs(0.5f * (one + z / (one - clampf(fb, 0.f, 0.95f) * z))) + 1e-6f);
}
void flg(const PicCtx& c) {
    const Box b = body(c);
    const float dep = clampf(par(c, 1), 0.f, 1.f), fb = par(c, 2);
    const float t0 = 1.f + 3.f * dep;  // ms around which the comb sweeps
    if (!c.small)
        traceUnit(c.g, b, [&](float u) { return dbUnit(combDb(logF(u), 1.f, fb) * 0.6f); }, PFaint, false, 2);
    traceUnit(c.g, b, [&](float u) { return dbUnit(combDb(logF(u), t0, fb) * 0.6f); }, PAccent, c.sel == 1 || c.sel == 2);
    if (c.small) return;
    char r[16], l[16];
    hzText(par(c, 0), r, sizeof r);
    snprintf(l, sizeof l, "FDB %d%%", (int)lroundf(fb * 100.f));
    captions(c, l, isSel(c, 2) ? PAccent : PDim, r, isSel(c, 0) ? PAccent : PText);
}
// ---- CHO: the two taps' delay over 2 s (RAT, DEP) ----
void cho(const PicCtx& c) {
    const float rate = clampf(par(c, 0), 0.05f, 5.f), dep = clampf(par(c, 1), 0.f, 1.f);
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 4);
    const float win = clampf(2.f / rate, 1.f, 10.f);
    trace(c.g, b, [&](float u) { return dep * 0.9f * sinf(6.2831853f * rate * win * u); }, c.sel <= 1 ? PAccent : PText, c.sel == 0 || c.sel == 1);
    if (c.small) return;
    trace(c.g, b, [&](float u) { return dep * 0.9f * sinf(6.2831853f * rate * win * u + 1.5707963f); }, PDim, false, 2);
    char r[16], w[12];
    hzText(par(c, 0), r, sizeof r);
    snprintf(w, sizeof w, "%.0fS", (double)win);
    captions(c, "TAP DELAYS", PDim, r, isSel(c, 0) ? PAccent : PText);
    label(c, w, c.x + c.w - 3, c.y + c.h - 7, PDim, Align::Right);
}
// ---- PSH: six harmonics in, the same shifted by SEM + FIN ----
void psh(const PicCtx& c) {
    const Box b = body(c);
    const float semis = par(c, 0) + par(c, 1) / 100.f, ratio = powf(2.f, semis / 12.f);
    const Box top = c.small ? band(b, 0.f, 0.45f) : band(b, 0.05f, 0.45f), low = band(b, 0.55f, 0.95f);
    auto xOf = [&](Box r, float h) { return r.x + (int)lroundf(clampf(log2f(h) / log2f(24.f), 0.f, 1.f) * (r.w - 1)); };  // log axis 1..24 x f0
    for (int h = 1; h <= 6; h++) {
        const int hh = (top.h - 1) * (7 - h) / 6;
        c.g.vline(xOf(top, (float)h * 1.5f), top.y + top.h - hh, hh, PDim);
        const float fh = (float)h * 1.5f * ratio;
        if (fh >= 1.f && fh <= 24.f) c.g.vline(xOf(low, fh), low.y + low.h - hh, hh, c.sel <= 1 ? PAccent : PText);
    }
    if (c.small) return;
    char r[16];
    snprintf(r, sizeof r, "%+.2f ST", (double)semis);
    captions(c, "IN / SHIFTED", PDim, r, c.sel <= 1 ? PAccent : PText);
}

// ---- DLY: the echoes on a 0..2 s ruler (TIM, FDB), the repeat count and the tail ----
void dly(const PicCtx& c) {
    const float t = par(c, 0) * 0.001f, fb = clampf(par(c, 1), 0.f, 0.95f), mix = clampf(par(c, 2), 0.f, 1.f);
    const Box b = c.small ? body(c) : Box{c.x + 4, c.y + 12, c.w - 8, c.h - 22};
    const float ruler = t * 6.f > 2.f ? (t * 6.f > 6.f ? 6.f : t * 6.f) : 2.f;
    c.g.hline(b.x, b.y + b.h - 1, b.w, PBorder);
    const int dh = (int)lroundf((1.f - mix) * (b.h - 2));
    c.g.fillRect(b.x, b.y + b.h - 1 - dh, 2, dh, PText);  // the dry
    int n = 0;
    float a = mix;
    for (int k = 1; k < 200 && a > 0.05f * mix && a > 0.002f; k++, a *= fb) {
        n++;
        const float tk = (float)k * t;
        if (tk > ruler) continue;
        const int x = b.x + (int)lroundf(tk / ruler * (b.w - 1)), h = (int)lroundf(clampf(a, 0.f, 1.f) * (b.h - 2));
        c.g.fillRect(x - 1, b.y + b.h - 1 - h, 2, h > 0 ? h : 1, c.sel < 0 || c.sel <= 1 ? PAccent : PText);
    }
    if (c.small) return;
    for (int s = 0; s <= (int)ruler; s++) {
        const int x = b.x + (int)lroundf((float)s / ruler * (b.w - 1));
        c.g.vline(x, b.y + b.h, 2, PDim);
        char l[6];
        snprintf(l, sizeof l, "%dS", s);
        if (s) label(c, l, x, b.y + b.h + 3, PDim, Align::Center);
    }
    char l[16], r[20];
    fmt(c, 0, l, sizeof l, true);
    const float tail = (float)n * t;
    snprintf(r, sizeof r, "%d ECHOES %.1fS", n, (double)tail);
    captions(c, l, isSel(c, 0) ? PAccent : PDim, r, isSel(c, 1) ? PAccent : PText);
}
// ---- REV: the tail (DEC), its pre-delay gap (PRE), early reflections (SIZ), damped highs (DMP) ----
void rev(const PicCtx& c) {
    const float size = clampf(par(c, 0), 0.3f, 1.f), dec = clampf(par(c, 1), 0.f, 0.97f), damp = clampf(par(c, 2), 0.f, 1.f);
    const float preS = par(c, 4) * 0.001f, loop = 0.12f * size;
    const float tau = dec > 0.01f ? -loop / logf(dec) : loop * 0.2f;  // e-fold time of the loop's feedback
    const float win = clampf(preS + 5.f * tau, 0.3f, 12.f);
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 3);
    auto env = [&](float u) { const float tt = u * win - preS; return tt < 0.f ? 0.f : expf(-tt / tau); };
    auto hi = [&](float u) { const float tt = u * win - preS; return tt < 0.f ? 0.f : expf(-tt / (tau * (1.f - 0.7f * damp))); };
    if (!c.small) {
        stipple(c.g, b, env);
        traceUnit(c.g, b, hi, isSel(c, 2) ? PAccent : PDim, false, 2);
        const int n = 2 + (int)lroundf(size * 4.f);  // early reflections in the first 80 ms
        mods::Rng rng;
        for (int k = 0; k < n; k++) {
            const float u = (preS + 0.08f * rng.uni()) / win;
            const int h = (int)lroundf((0.3f + 0.6f * rng.uni()) * (b.h - 1));
            dashV(c.g, xUnit(b, u), b.y + b.h - h, h, isSel(c, 0) ? PAccent : PDim, 1, 1);
        }
    }
    traceUnit(c.g, b, env, c.sel < 0 || c.sel == 1 || c.sel == 4 ? PAccent : PText, isSel(c, 1));
    if (c.small) return;
    char l[16], r[16];
    snprintf(l, sizeof l, "PRE %dMS", (int)lroundf(preS * 1000.f));
    snprintf(r, sizeof r, "TAIL %.1fS", (double)(5.f * tau));
    captions(c, l, isSel(c, 4) ? PAccent : PDim, r, isSel(c, 1) ? PAccent : PText);
}

// ---- VOW: the three formant peaks at this vowel (SHF moves them), the vowel's letter ----
void vowPic(const PicCtx& c) {
    float f[3];
    vowFormants(par(c, 0), f);
    const float shift = powf(2.f, par(c, 1) / 12.f), q = 1.f / (0.25f - 0.21f * clampf(par(c, 2), 0.f, 1.f));  // the Svf's Q = 1 / damp
    static const float kG[3] = {0.f, -6.f, -12.f};
    auto resp = [&](float u) {
        const float hz = logF(u);
        float best = -60.f;
        for (int k = 0; k < 3; k++) {
            const float d = bpDb(hz, f[k] * shift, q) + kG[k];
            best = d > best ? d : best;
        }
        return dbUnit(best);
    };
    const Box b = body(c);
    if (!c.small)
        for (float hz : {100.f, 1000.f, 10000.f}) dashV(c.g, xUnit(b, uOfF(hz)), b.y, b.h, PBorder, 1, 2);
    traceUnit(c.g, b, resp, c.sel < 0 || c.sel <= 2 ? PAccent : PText, c.sel == 0 || c.sel == 1);
    if (c.small) return;
    static const char* const kV[5] = {"A", "E", "I", "O", "U"};
    const float v = clampf(par(c, 0), 0.f, 4.f);
    const int a = (int)v < 4 ? (int)v : 3;
    char l[16];
    if (v - (float)a < 0.05f) snprintf(l, sizeof l, "%s", kV[a]);
    else if (v - (float)a > 0.95f) snprintf(l, sizeof l, "%s", kV[a + 1]);
    else snprintf(l, sizeof l, "%s > %s", kV[a], kV[a + 1]);
    char r[16];
    snprintf(r, sizeof r, "F1 %d", (int)lroundf(f[0] * shift));
    captions(c, l, isSel(c, 0) ? PAccent : PText, r, isSel(c, 1) ? PAccent : PDim);
}
// ---- EQ3: the summed response of its three biquads (the module's own design) ----
void eq3Pic(const PicCtx& c) {
    Biquad bq[3];
    eq3Design(par(c, 0), par(c, 1), par(c, 2), par(c, 3), bq);
    const Box b = body(c);
    auto resp = [&](float u) {  // +-18 dB across the box
        const float hz = logF(u);
        const float db = bq[0].magDb(hz, 48000.f) + bq[1].magDb(hz, 48000.f) + bq[2].magDb(hz, 48000.f);
        return clampf(db / 18.f, -1.f, 1.f);
    };
    if (!c.small) {
        for (float hz : {100.f, 1000.f, 10000.f}) dashV(c.g, xUnit(b, uOfF(hz)), b.y, b.h, PBorder, 1, 2);
        dashH(c.g, b.x, b.y + b.h / 2, b.w, PBorder, 1, 2);
        dashV(c.g, xUnit(b, uOfF(par(c, 2))), b.y, b.h, isSel(c, 2) ? PAccent : PDim);
    }
    trace(c.g, b, resp, PAccent, c.sel >= 0);
    if (c.small) return;
    char l[28];
    snprintf(l, sizeof l, "%+.0f %+.0f %+.0f DB", (double)par(c, 0), (double)par(c, 1), (double)par(c, 3));
    char r[12];
    hzText(par(c, 2), r, sizeof r);
    captions(c, l, c.sel >= 0 ? PAccent : PDim, r, isSel(c, 2) ? PAccent : PText);
}

}  // namespace

const Entry kProcPics[] = {
    {"vcf", vcf, vcfGlyph}, {"trm", trm, nullptr}, {"lpg", lpg, nullptr}, {"fld", fld, nullptr},
    {"ovd", ovd, nullptr},  {"cru", cru, nullptr}, {"atn", atn, nullptr}, {"dyn", dyn, nullptr},
    {"ring", ring, nullptr}, {"xfd", xfd, nullptr}, {"lad", lad, ladGlyph}, {"awh", awh, nullptr},
    {"pha", pha, nullptr},  {"flg", flg, nullptr}, {"cho", cho, nullptr}, {"psh", psh, nullptr},
    {"dly", dly, nullptr},  {"rev", rev, nullptr}, {"vow", vowPic, nullptr}, {"eq3", eq3Pic, nullptr},
};
const int kNumProcPics = sizeof kProcPics / sizeof kProcPics[0];

}  // namespace pic
