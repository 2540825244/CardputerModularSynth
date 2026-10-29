// Pictures: envelopes and modulators.
#include <cstdio>
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

}  // namespace

const Entry kModPics[] = {
    {"env", env, envGlyph},
};
const int kNumModPics = sizeof kModPics / sizeof kModPics[0];

}  // namespace pic
