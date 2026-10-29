// Pictures: oscillators, voices, drums.
#include <cstdio>
#include "Synthesis/fm2.h"
#include "Synthesis/formantosc.h"
#include "Synthesis/variableshapeosc.h"
#include "Synthesis/vosim.h"
#include "Synthesis/zoscillator.h"
#include "core/modules/common.h"
#include "core/units.h"
#include "pictures_internal.h"

namespace pic {
namespace {

constexpr float kFs = 48000.f;

// The pitch the OCT / TUNE (/ FINE) knobs set with no pitch CV, as a note name.
void baseNote(const PicCtx& c, int octIdx, float cents, char* out, int len) {
    noteName(par(c, octIdx) * 12.f + par(c, octIdx + 1) + cents / 100.f, out, len);
}
float baseHz(const PicCtx& c, int octIdx) {
    return kC4Hz * powf(2.f, par(c, octIdx) + par(c, octIdx + 1) / 12.f);
}

// The real oscillator, run for `cycles` cycles of f0 and drawn sample by sample across the box
// (a few hundred to ~1500 Process() calls, on a param change only). out = -1..1-ish.
template <class Step>
void scope(Gfx& g, Box b, float f0, float cycles, Step step, uint8_t col, bool bold, float gain = 1.f / 1.25f) {
    f0 = clampf(f0, 32.f, 2000.f);
    const int n = (int)(kFs * cycles / f0);
    int px = b.x, py = ySigned(b, step() * gain);
    for (int i = 1; i < n; i++) {
        const int x = b.x + i * (b.w - 1) / (n - 1), y = ySigned(b, step() * gain);
        g.line(px, py, x, y, col);
        if (bold) g.line(px, py < b.y + b.h - 1 ? py + 1 : py, x, y < b.y + b.h - 1 ? y + 1 : y, col);
        px = x;
        py = y;
    }
}
// The pitch the scope pictures are drawn at: the knobs' note, kept where 2 cycles stay readable.
float scopeHz(const PicCtx& c, int octIdx) { return clampf(baseHz(c, octIdx), 65.f, 1050.f); }

// Bars of amplitudes amp[0..n) (0..1) along the box, the first bar strongest ink.
void bars(Gfx& g, Box b, const float* amp, int n, uint8_t col, uint8_t col0) {
    const int bw = b.w / n;
    for (int i = 0; i < n; i++) {
        const int h = (int)lroundf(clampf(amp[i], 0.f, 1.f) * (b.h - 1));
        if (h > 0) g.fillRect(b.x + i * bw + (bw > 3 ? 1 : 0), b.y + b.h - h, bw > 3 ? bw - 2 : (bw > 1 ? bw - 1 : 1), h, i ? col : col0);
    }
}

// The top part of a two-part picture (wave over spectrum / bars) and the bottom part.
Box topPart(const PicCtx& c, float split) { const Box b = body(c); return c.small ? b : band(b, 0.f, split); }
Box lowPart(const PicCtx& c, float split) { const Box b = body(c); return band(b, split + 0.06f, 1.f); }

// ---- OSC: the wave, its harmonics, and the note it plays ----
void harmonicsOf(int kind, float* amp, int n) {
    for (int h = 1; h <= n; h++) {
        float a = 0.f;
        if (kind == 0) a = h == 1 ? 1.f : 0.f;
        else if (kind == 1) a = h % 2 ? 1.f / (float)(h * h) : 0.f;
        else if (kind == 2) a = 1.f / (float)h;
        else a = h % 2 ? 1.f / (float)h : 0.f;
        amp[h - 1] = a;
    }
}
void osc(const PicCtx& c) {
    const int kind = ipar(c, 0);
    const Box w = topPart(c, 0.58f);
    if (!c.small) dashH(c.g, w.x, w.y + w.h / 2, w.w, PBorder, 1, 2);
    trace(c.g, insetXY(w, 0, 2), [&](float u) { return basicWave(kind, u * 2.5f + 0.0001f); },
          c.sel <= 0 ? PAccent : PText, isSel(c, 0));
    if (c.small) return;
    float amp[12];
    harmonicsOf(kind, amp, 12);
    bars(c.g, lowPart(c, 0.58f), amp, 12, PDim, PText);
    char note[16];
    const float hz = mon(c, 0, 0.f);
    if (hz > 1.f) noteName(12.f * log2f(hz / kC4Hz), note, sizeof note);
    else baseNote(c, 1, par(c, 3), note, sizeof note);
    captions(c, enumName(c, 0), isSel(c, 0) ? PAccent : PDim, note, c.sel >= 1 ? PAccent : PText);
}
bool oscGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    const int kind = (int)lroundf(m.params[0]);
    trace(g, b, [&](float u) { return basicWave(kind, u * 1.5f + 0.0001f) * 0.9f; }, col);
    return true;
}

// ---- VSO: DaisySP's variable-shape osc (saw/tri <-> square, PW, hard sync) ----
enum { kVOct, kVTune, kVShape, kVPw, kVSync, kVRatio };
void vsoTrace(const ModuleRec& m, Gfx& g, Box b, float f0, uint8_t col, bool bold) {
    daisysp::VariableShapeOscillator o;
    o.Init(kFs);
    const bool sync = m.params[kVSync] >= 0.5f;
    o.SetSync(sync);
    if (sync) {
        o.SetFreq(f0);
        o.SetSyncFreq(f0 * m.params[kVRatio]);
    } else {
        o.SetSyncFreq(f0);
    }
    o.SetWaveshape(clampf(m.params[kVShape], 0.f, 1.f));
    o.SetPW(clampf(m.params[kVPw], 0.02f, 0.98f));
    for (int i = 0; i < 64; i++) o.Process();  // settle the band-limiting
    scope(g, b, f0, 2.f, [&] { return o.Process(); }, col, bold);
}
void vso(const PicCtx& c) {
    const Box w = body(c);
    const Box tb = c.small ? w : insetXY(w, 0, 3);
    const float f0 = 200.f;  // the shape, not the pitch: sync ratios up to 8 stay visible
    if (!c.small && par(c, kVSync) >= 0.5f)
        for (int k = 0; k <= 2; k++) dashV(c.g, tb.x + k * (tb.w - 1) / 2, w.y, w.h, PDim, 1, 2);
    vsoTrace(c.m, c.g, tb, f0, PAccent, c.sel == kVShape || c.sel == kVPw || c.sel == kVRatio);
    if (c.small) return;
    char l[24], note[16];
    if (par(c, kVSync) >= 0.5f) snprintf(l, sizeof l, "SYNC X%.2f", (double)par(c, kVRatio));
    else snprintf(l, sizeof l, "SHP %d%% PW %d%%", (int)lroundf(par(c, kVShape) * 100.f), (int)lroundf(par(c, kVPw) * 100.f));
    baseNote(c, kVOct, 0.f, note, sizeof note);
    captions(c, l, c.sel >= kVShape ? PAccent : PDim, note, c.sel == kVOct || c.sel == kVTune ? PAccent : PText);
}
bool vsoGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    vsoTrace(m, g, b, 200.f, col, false);
    return true;
}

// ---- FMO: the real DaisySP Fm2 wave. Its modulator accumulates into the carrier's phase, so
// even small indexes are bright; the picture shows that as it is. ----
void fmo(const PicCtx& c) {
    const float ratio = par(c, 2), index = clampf(par(c, 3), 0.f, 1.f) * 8.f;
    daisysp::Fm2 o;
    o.Init(kFs);
    const float f0 = scopeHz(c, 0);
    o.SetFrequency(f0);
    o.SetRatio(ratio);
    o.SetIndex(index);
    const Box b = body(c);
    scope(c.g, c.small ? b : insetXY(b, 0, 3), f0, 1.5f, [&] { return o.Process(); }, PAccent, c.sel == 2 || c.sel == 3);
    if (c.small) return;
    char l[28], note[16];
    snprintf(l, sizeof l, "RAT %.2f IDX %d%%", (double)ratio, (int)lroundf(par(c, 3) * 100.f));
    baseNote(c, 0, 0.f, note, sizeof note);
    captions(c, l, c.sel == 2 || c.sel == 3 ? PAccent : PDim, note, c.sel == 0 || c.sel == 1 ? PAccent : PText);
}

// ---- FOR / VOS / ZOS: the real formant oscillators ----
void formantCaption(const PicCtx& c, int fIdx, int octIdx) {
    char l[24], f[12], note[16];
    fmt(c, fIdx, f, sizeof f);
    snprintf(l, sizeof l, "FORMANT %s", f);
    baseNote(c, octIdx, 0.f, note, sizeof note);
    captions(c, l, isSel(c, fIdx) ? PAccent : PDim, note, c.sel == octIdx || c.sel == octIdx + 1 ? PAccent : PText);
}
void forPic(const PicCtx& c) {
    daisysp::FormantOscillator o;
    o.Init(kFs);
    const float f0 = scopeHz(c, 0);
    o.SetCarrierFreq(f0);
    o.SetFormantFreq(clampf(par(c, 2), 50.f, 12000.f));
    o.SetPhaseShift(par(c, 3));
    const Box b = body(c);
    scope(c.g, c.small ? b : insetXY(b, 0, 3), f0, 2.f, [&] { return o.Process(); }, PAccent, c.sel >= 2);
    if (!c.small) formantCaption(c, 2, 0);
}
void vosPic(const PicCtx& c) {
    daisysp::VosimOscillator o;
    o.Init(kFs);
    const float f0 = scopeHz(c, 0);
    o.SetFreq(f0);
    o.SetForm1Freq(par(c, 2));
    o.SetForm2Freq(par(c, 3));
    o.SetShape(par(c, 4) * 2.f - 1.f);
    const Box b = body(c);
    scope(c.g, c.small ? b : insetXY(b, 0, 3), f0, 2.f, [&] { return o.Process(); }, PAccent, c.sel >= 2);
    if (c.small) return;
    char l[28], f1[12], f2[12], note[16];
    fmt(c, 2, f1, sizeof f1);
    fmt(c, 3, f2, sizeof f2);
    snprintf(l, sizeof l, "F1 %s F2 %s", f1, f2);
    baseNote(c, 0, 0.f, note, sizeof note);
    captions(c, l, c.sel == 2 || c.sel == 3 ? PAccent : PDim, note, c.sel == 0 || c.sel == 1 ? PAccent : PText);
}
void zosPic(const PicCtx& c) {
    daisysp::ZOscillator o;
    o.Init(kFs);
    const float f0 = scopeHz(c, 0);
    o.SetFreq(f0);
    o.SetFormantFreq(par(c, 2));
    o.SetShape(par(c, 3));
    o.SetMode(par(c, 4) * 2.f - 1.f);
    const Box b = body(c);
    scope(c.g, c.small ? b : insetXY(b, 0, 3), f0, 2.f, [&] { return o.Process(); }, PAccent, c.sel >= 2);
    if (!c.small) formantCaption(c, 2, 0);
}

// ---- SSW: the 7 saws as a fan around the pitch (DET spreads them, MIX weighs the six) ----
const float kSswSpread[7] = {0.f, -0.11002f, 0.10929f, -0.06288f, 0.06123f, -0.01953f, 0.01953f};
void ssw(const PicCtx& c) {
    const float det = clampf(par(c, 2), 0.f, 1.f), mix = clampf(par(c, 3), 0.f, 1.f);
    const Box w = topPart(c, 0.4f);
    trace(c.g, c.small ? w : insetXY(w, 0, 2), [&](float u) { return basicWave(2, u * 3.f + 0.0001f) * 0.9f; },
          c.small ? PAccent : PDim);
    if (c.small) return;
    const Box f = lowPart(c, 0.4f);
    const int cx = f.x + f.w / 2;
    c.g.hline(f.x, f.y + f.h - 1, f.w, PBorder);
    for (int v = 0; v < 7; v++) {
        const float a = v == 0 ? 1.f - 0.5f * mix : mix * 0.3f / 0.55f;  // the six drawn up to the centre's height
        const int x = cx + (int)lroundf(kSswSpread[v] * det * det * (f.w / 2 - 2) / 0.11f);
        const int h = (int)lroundf(clampf(a, 0.05f, 1.f) * (f.h - 2));
        const uint8_t col = v == 0 ? (isSel(c, 3) ? PAccent : PText) : (isSel(c, 2) ? PAccent : (c.sel < 0 ? PAccent : PDim));
        if (v == 0) c.g.fillRect(x - 1, f.y + f.h - 1 - h, 3, h, col);
        else c.g.vline(x, f.y + f.h - 1 - h, h, col);
    }
    char l[24], note[16];
    snprintf(l, sizeof l, "DETUNE %d%%", (int)lroundf(det * 100.f));
    baseNote(c, 0, 0.f, note, sizeof note);
    captions(c, l, isSel(c, 2) ? PAccent : PDim, note, c.sel == 0 || c.sel == 1 ? PAccent : PText);
}

// ---- HRM: the 16 harmonics are the UI; the summed wave above them ----
void hrmAmps(const ModuleRec& m, float* amp) {
    const float center = clampf(m.params[2], 1.f, 16.f), roll = m.params[3], odd = m.params[4];
    float peak = 0.f;
    for (int h = 0; h < 16; h++) {
        const float d = fabsf((float)(h + 1) - center);
        amp[h] = powf(roll, d) * (((h + 1) % 2 == 0) ? 1.f - odd : 1.f);
        peak = amp[h] > peak ? amp[h] : peak;
    }
    for (int h = 0; h < 16; h++) amp[h] = peak > 0.f ? amp[h] / peak : 0.f;
}
void hrm(const PicCtx& c) {
    float amp[16];
    hrmAmps(c.m, amp);
    if (c.small) {
        bars(c.g, body(c), amp, 16, PAccent, PAccent);
        return;
    }
    const Box w = band(body(c), 0.f, 0.32f);
    float sum = 0.f;
    for (float a : amp) sum += a;
    trace(c.g, w, [&](float u) {
        float v = 0.f;
        for (int h = 0; h < 16; h++) v += amp[h] * sinf(6.2831853f * (float)(h + 1) * u * 2.f);
        return v / (sum > 0.f ? sum : 1.f);
    }, PDim);
    const Box bb = band(body(c), 0.38f, 1.f);
    bars(c.g, bb, amp, 16, c.sel < 0 || c.sel >= 3 ? PAccent : PText, c.sel < 0 || c.sel >= 3 ? PAccent : PText);
    const int bw = bb.w / 16, cx = bb.x + (int)lroundf((clampf(par(c, 2), 1.f, 16.f) - 0.5f) * bw);
    dashV(c.g, cx, bb.y, bb.h, isSel(c, 2) ? PAccent : PDim, 1, 1);
    char l[20], note[16];
    snprintf(l, sizeof l, "CENTER %d", (int)lroundf(par(c, 2)));
    baseNote(c, 0, 0.f, note, sizeof note);
    captions(c, l, isSel(c, 2) ? PAccent : PDim, note, c.sel == 0 || c.sel == 1 ? PAccent : PText);
}

// ---- NOI: a fixed stretch of each kind (seeded, so it does not crawl) ----
void noi(const PicCtx& c) {
    const int kind = ipar(c, 0);
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 2);
    mods::Rng rng;
    float b0 = 0.f, b1 = 0.f, b2 = 0.f, brown = 0.f, ph = 0.f, held = 0.f, crk = 0.f;
    const float rate = par(c, 1) / kFs;
    const int n = 480;  // 10 ms
    int px = b.x, py = ySigned(b, 0.f);
    for (int i = 0; i < n; i++) {
        const float w = rng.bi();
        float y;
        switch (kind) {
            case 1:
                b0 = 0.99765f * b0 + w * 0.0990460f;
                b1 = 0.96300f * b1 + w * 0.2965164f;
                b2 = 0.57000f * b2 + w * 1.0526913f;
                y = (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
                break;
            case 2: y = (brown = clampf(brown * 0.998f + w * 0.05f, -1.f, 1.f)) * 3.f; break;
            case 3:
                ph += rate;
                if (ph >= 1.f) {
                    ph -= floorf(ph);
                    held = w;
                }
                y = held;
                break;
            case 4: y = rng.uni() < rate ? (w > 0.f ? 1.f : -1.f) : 0.f; break;
            case 5: {
                const float imp = rng.uni() < rate * 0.2f ? w : 0.f;
                crk = crk * 0.9f + imp;
                y = clampf(crk, -1.f, 1.f);
                break;
            }
            default: y = w; break;
        }
        const int x = b.x + i * (b.w - 1) / (n - 1), yy = ySigned(b, y * 0.9f);
        if (i) c.g.line(px, py, x, yy, PAccent);
        px = x;
        py = yy;
    }
    if (c.small) return;
    char r[12];
    fmt(c, 1, r, sizeof r, true);
    captions(c, enumName(c, 0), isSel(c, 0) ? PAccent : PDim, kind >= 3 ? r : "10 MS", isSel(c, 1) ? PAccent : PDim);
}

}  // namespace

const Entry kSourcePics[] = {
    {"osc", osc, oscGlyph}, {"vso", vso, vsoGlyph}, {"fmo", fmo, nullptr}, {"for", forPic, nullptr},
    {"vos", vosPic, nullptr}, {"zos", zosPic, nullptr}, {"ssw", ssw, nullptr}, {"hrm", hrm, nullptr},
    {"noi", noi, nullptr},
};
const int kNumSourcePics = sizeof kSourcePics / sizeof kSourcePics[0];

}  // namespace pic
