// Pictures: envelopes and modulators.
#include <cstdio>
#include "core/modules/common.h"
#include "pictures_internal.h"

namespace pic {
namespace {

// Stipple under a unit curve: every third pixel, so the fill reads without a colour of its own.
template <class F>
void stipple(Gfx& g, Box b, F f) {
    for (int px = 1; px < b.w - 1; px += 3) {
        const int top = yUnit(b, f((float)px / (float)(b.w - 1)));
        for (int yy = b.y + b.h - 2; yy > top + 1; yy -= 3) g.pixel(b.x + px, yy, PBorder);
    }
}

// ---- ENV: ADSR. Segment widths grow with sqrt(time) so a 2 ms attack still shows; the
// sustain plateau is a fixed fifth. ----
struct Adsr {
    float xa, xd, xs;  // segment ends, 0..1 (release ends at 1)
    float s;
};
Adsr adsrShape(float a, float d, float s, float r) {
    const float ka = sqrtf(a), kd = sqrtf(d), kr = sqrtf(r), sum = ka + kd + kr;
    const float flow = 0.8f;
    Adsr e;
    e.xa = flow * ka / sum;
    e.xd = e.xa + flow * kd / sum;
    e.xs = e.xd + (1.f - flow);
    e.s = s;
    return e;
}
float adsrAt(const Adsr& e, float u) {
    if (u < e.xa) return e.xa > 0.f ? u / e.xa : 1.f;
    if (u < e.xd) return 1.f - (1.f - e.s) * (u - e.xa) / (e.xd - e.xa);
    if (u < e.xs) return e.s;
    return e.s * (1.f - (u - e.xs) / (1.f - e.xs));
}

void env(const PicCtx& c) {
    const Box all = body(c);
    const Box b = c.small ? all : Box{all.x, all.y + 2, all.w, all.h - 8};
    const ParamSpec* sp = c.m.type->params;
    const Adsr e = adsrShape(par(c, 0), par(c, 1), par(c, 2), par(c, 3));
    const Adsr gst = adsrShape(sp[0].def, sp[1].def, sp[2].def, sp[3].def);
    if (!c.small) {
        stipple(c.g, b, [&](float u) { return adsrAt(e, u); });
        traceUnit(c.g, b, [&](float u) { return adsrAt(gst, u); }, PFaint, false, 2);
    }
    // each segment in its own ink: the selected one bold accent
    const float ends[5] = {0.f, e.xa, e.xd, e.xs, 1.f};
    for (int seg = 0; seg < 4; seg++) {
        const Box sb = {xUnit(b, ends[seg]), b.y, xUnit(b, ends[seg + 1]) - xUnit(b, ends[seg]) + 1, b.h};
        if (sb.w < 1) continue;
        const uint8_t col = c.sel < 0 ? PAccent : ink(c, seg);
        const float u0 = ends[seg], du = ends[seg + 1] - ends[seg];
        traceUnit(c.g, sb, [&](float u) { return adsrAt(e, u0 + u * du); }, col, isSel(c, seg));
    }
    if (c.small) return;
    c.g.hline(b.x, b.y + b.h - 1, b.w, PBorder);
    static const char* const kSeg[] = {"A", "D", "S", "R"};
    const float mid[4] = {e.xa / 2.f, (e.xa + e.xd) / 2.f, (e.xd + e.xs) / 2.f, (e.xs + 1.f) / 2.f};
    for (int i = 0; i < 4; i++) label(c, kSeg[i], xUnit(b, mid[i]), b.y + b.h + 1, isSel(c, i) ? PAccent : PFaint, Align::Center);
    char t[16];
    const float ms = par(c, 0) + par(c, 1) + par(c, 3);
    if (ms >= 1000.f) snprintf(t, sizeof t, "%.1fS", (double)(ms / 1000.f));
    else snprintf(t, sizeof t, "%dMS", (int)lroundf(ms));
    captions(c, "ADSR", PDim, t, PText);
    // live: where the envelope is now (stage 1..4 = A D S R, level 0..1)
    const int stage = (int)mon(c, 0, 0.f);
    const float lvl = clampf(mon(c, 1, 0.f), 0.f, 1.f);
    if (stage < 1 || stage > 4) return;
    float u;
    if (stage == 1) u = e.xa * lvl;
    else if (stage == 2) u = e.xa + (e.xd - e.xa) * (e.s < 1.f ? (1.f - lvl) / (1.f - e.s) : 1.f);
    else if (stage == 3) u = (e.xd + e.xs) / 2.f;
    else u = e.xs + (1.f - e.xs) * (e.s > 0.f ? 1.f - lvl / e.s : 1.f);
    const int dx = xUnit(b, clampf(u, 0.f, 1.f)), dy = yUnit(b, lvl);
    c.g.fillCircle(dx, dy < b.y + 2 ? b.y + 2 : dy, 2, PText);
}
bool envGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    const Adsr e = adsrShape(m.params[0], m.params[1], m.params[2], m.params[3]);
    traceUnit(g, b, [&](float u) { return adsrAt(e, u); }, col);
    return true;
}


// ---- LFO: two cycles of the shape, the 90 output dashed, the phase dot (live) ----
float lfoWave(int w, float t) {
    static const float kHeld[4] = {0.6f, -0.45f, 0.9f, -0.8f};  // S&H / SMO: a fixed example
    const int cyc = (int)floorf(t) & 3;
    const float p = t - floorf(t);
    switch (w) {
        case 1: return p < 0.5f ? 1.f : -1.f;
        case 2: return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
        case 3: return 2.f * p - 1.f;
        case 4: return 1.f - 2.f * p;
        case 5: return kHeld[cyc];
        case 6: {
            const float k = 0.5f - 0.5f * cosf(3.1415927f * p);
            return kHeld[cyc] + (kHeld[(cyc + 1) & 3] - kHeld[cyc]) * k;
        }
        default: return sinf(6.2831853f * p);
    }
}
void lfo(const PicCtx& c) {
    const int w = ipar(c, 0);
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 4);
    if (!c.small) {
        grid(c, b, 4, 2);
        trace(c.g, b, [&](float u) { return lfoWave(w, u * 2.f + 0.25f); }, PDim, false, 2);
    }
    trace(c.g, b, [&](float u) { return lfoWave(w, u * 2.f); }, c.sel <= 0 ? PAccent : PText, isSel(c, 0));
    if (c.small) return;
    if (live(c)) {
        const float ph = clampf(mon(c, 0, 0.f), 0.f, 0.999f);
        const int x = xUnit(b, ph * 0.5f), y = ySigned(b, lfoWave(w, ph));
        c.g.fillCircle(x, y, 3, PBg);
        c.g.circle(x, y, 3, PText);
        c.g.fillCircle(x, y, 1, PText);
    }
    char r[24];
    const float hz = par(c, 1);
    if (hz > 1.f) snprintf(r, sizeof r, "%.2fHZ %dMS", (double)hz, (int)lroundf(1000.f / hz));
    else snprintf(r, sizeof r, "%.2fHZ %.1fS", (double)hz, (double)(1.f / hz));
    captions(c, enumName(c, 0), isSel(c, 0) ? PAccent : PDim, r, isSel(c, 1) ? PAccent : PText);
    label(c, "- - 90", c.x + c.w - 3, c.y + c.h - 7, PDim, Align::Right);
}
bool lfoGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    const int w = (int)lroundf(m.params[0]);
    trace(g, b, [&](float u) { return lfoWave(w, u * 1.5f) * 0.9f; }, col);
    return true;
}

// ---- RND: a window of 6+ steps: the stepped value and the smoothed one ----
void rnd(const PicCtx& c) {
    const float rate = clampf(par(c, 0), 0.05f, 50.f), range = clampf(par(c, 2) / 5.f, 0.f, 1.f);
    const Box b = c.small ? body(c) : insetXY(body(c), 0, 3);
    const float secs = rate * 1.f >= 6.f ? 1.f : ceilf(6.f / rate);  // at least 6 steps
    const float tau = 0.002f + par(c, 1) / rate;
    const float k = 1.f - expf(-secs / (tau * (float)(b.w - 1)));
    mods::Rng rng;
    float target = rng.bi(), y = target, ph = 0.f;
    int pyS = ySigned(b, target * range), prev = pyS;
    for (int px = 1; px < b.w; px++) {
        ph += rate * secs / (float)(b.w - 1);
        if (ph >= 1.f) {
            ph -= floorf(ph);
            target = rng.bi();
        }
        y += k * (target - y);
        const int ys = ySigned(b, target * range), yv = ySigned(b, y * range);
        if (!c.small) c.g.line(b.x + px - 1, pyS, b.x + px, ys, PDim);
        c.g.line(b.x + px - 1, prev, b.x + px, yv, c.sel < 0 || c.sel == 1 ? PAccent : PText);
        pyS = ys;
        prev = yv;
    }
    if (c.small) return;
    char r[20];
    fmt(c, 0, r, sizeof r, true);
    char w[16];
    snprintf(w, sizeof w, "%d S", (int)secs);
    captions(c, w, PDim, r, isSel(c, 0) ? PAccent : PText);
}

// ---- CHS: the Lorenz attractor at this RHO (X against Z), the live point ----
void chs(const PicCtx& c) {
    const float rho = clampf(par(c, 1), 10.f, 40.f);
    const Box b = c.small ? body(c) : insetXY(body(c), 2, 2);
    float x = 1.f, y = 1.f, z = 1.f;
    const uint8_t col = c.sel < 0 || c.sel == 1 ? PAccent : PText;
    auto px = [&](float xx) { return b.x + (int)lroundf(clampf((xx + 25.f) / 50.f, 0.f, 1.f) * (b.w - 1)); };
    auto pz = [&](float zz) { return b.y + (int)lroundf((1.f - clampf(zz / 60.f, 0.f, 1.f)) * (b.h - 1)); };
    const int steps = c.small ? 1200 : 3000;
    int lx = px(x), lz = pz(z);
    for (int i = 0; i < steps; i++) {
        const float dt = 0.008f;
        const float dx = 10.f * (y - x), dy = x * (rho - z) - y, dz = x * y - (8.f / 3.f) * z;
        x += dx * dt;
        y += dy * dt;
        z += dz * dt;
        if (!(fabsf(x) < 100.f && fabsf(y) < 100.f && fabsf(z) < 100.f)) x = y = z = 1.f;
        const int nx = px(x), nz = pz(z);
        if (i > 100) c.g.line(lx, lz, nx, nz, i % 3 ? col : PDim);
        lx = nx;
        lz = nz;
    }
    if (c.small) return;
    if (live(c)) {  // monitor 0 = x, 1 = z (chs.cpp)
        const int dx = px(mon(c, 0, 0.f)), dz = pz(mon(c, 1, 0.f));
        c.g.fillCircle(dx, dz, 2, PText);
    }
    char r[16];
    snprintf(r, sizeof r, "RHO %d", (int)lroundf(rho));
    captions(c, "X / Z", PDim, r, isSel(c, 1) ? PAccent : PText);
}

}  // namespace

const Entry kModPics[] = {
    {"env", env, envGlyph}, {"lfo", lfo, lfoGlyph}, {"rnd", rnd, nullptr}, {"chs", chs, nullptr},
};
const int kNumModPics = sizeof kModPics / sizeof kModPics[0];

}  // namespace pic
