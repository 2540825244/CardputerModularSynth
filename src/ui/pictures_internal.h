#pragma once
// Shared helpers for the painters (ui/pic_*.cpp). Not for the rest of the UI.
#include <cmath>
#include <cstdio>
#include "core/param.h"
#include "pictures.h"
#include "render/palette.h"

namespace pic {

struct Box {
    int x, y, w, h;
};
inline Box box(const PicCtx& c) { return {c.x, c.y, c.w, c.h}; }
inline Box inset(Box b, int d) { return {b.x + d, b.y + d, b.w - 2 * d, b.h - 2 * d}; }
inline Box insetXY(Box b, int dx, int dy) { return {b.x + dx, b.y + dy, b.w - 2 * dx, b.h - 2 * dy}; }
// Horizontal bands: rows [from, to) of the box as fractions of its height.
inline Box band(Box b, float from, float to) {
    const int y0 = b.y + (int)(b.h * from), y1 = b.y + (int)(b.h * to);
    return {b.x, y0, b.w, y1 - y0};
}
inline Box cols(Box b, float from, float to) {
    const int x0 = b.x + (int)(b.w * from), x1 = b.x + (int)(b.w * to);
    return {x0, b.y, x1 - x0, b.h};
}

inline float clampf(float v, float lo, float hi) { return v != v ? lo : (v < lo ? lo : (v > hi ? hi : v)); }
inline float par(const PicCtx& c, int i) { return c.m.params[i]; }
inline float norm(const PicCtx& c, int i) { return paramToNorm(c.m.type->params[i], c.m.params[i]); }
inline int ipar(const PicCtx& c, int i) { return (int)lroundf(c.m.params[i]); }
inline float mon(const PicCtx& c, int i, float dflt) { return c.inst ? c.inst->monitor(i) : dflt; }
inline bool live(const PicCtx& c) { return c.inst != nullptr; }

// Emphasis: the selected param's part of the picture is the accent; with nothing (or a pin)
// selected the main trace is.
inline bool isSel(const PicCtx& c, int param) { return c.sel == param; }
inline uint8_t ink(const PicCtx& c, int param) { return c.sel == param ? PAccent : PText; }
inline uint8_t mainInk(const PicCtx& c, int a = -1, int b = -1, int d = -1) {
    return c.sel < 0 || c.sel == a || c.sel == b || c.sel == d ? PAccent : PText;
}

// y of a value -1..1 (top = 1) / 0..1 in the box; NaN and out-of-range clamp to the edges.
inline int ySigned(Box b, float v) { return b.y + (int)lroundf((1.f - (clampf(v, -1.f, 1.f) + 1.f) * 0.5f) * (b.h - 1)); }
inline int yUnit(Box b, float v) { return b.y + (int)lroundf((1.f - clampf(v, 0.f, 1.f)) * (b.h - 1)); }
inline int xUnit(Box b, float u) { return b.x + (int)lroundf(clampf(u, 0.f, 1.f) * (b.w - 1)); }

// Values a painter computed that were not finite (NaN / inf): drawing clamps them to an edge, so
// the tests read this to catch them (test_pictures).
extern int gNonFinite;
inline float finite(float v) {
    if (!std::isfinite(v)) {
        gNonFinite++;
        return 0.f;
    }
    return v;
}

// A polyline of f(u), u = 0..1 across the box, f -1..1. bold doubles it one pixel down (inside
// the box); dash > 0 draws dash pixels on, dash off.
template <class F>
void trace(Gfx& g, Box b, F f, uint8_t col, bool bold = false, int dash = 0) {
    if (b.w < 2 || b.h < 2) return;
    int prev = ySigned(b, finite(f(0.f)));
    for (int px = 1; px < b.w; px++) {
        const int yy = ySigned(b, finite(f((float)px / (float)(b.w - 1))));
        if (!dash || (px / dash) % 2 == 0) {
            g.line(b.x + px - 1, prev, b.x + px, yy, col);
            if (bold) g.line(b.x + px - 1, prev < b.y + b.h - 1 ? prev + 1 : prev, b.x + px, yy < b.y + b.h - 1 ? yy + 1 : yy, col);
        }
        prev = yy;
    }
}
// The same for f 0..1 (bottom = 0).
template <class F>
void traceUnit(Gfx& g, Box b, F f, uint8_t col, bool bold = false, int dash = 0) {
    trace(g, b, [&](float u) { return f(u) * 2.f - 1.f; }, col, bold, dash);
}

inline void dashH(Gfx& g, int x, int y, int w, uint8_t col, int on = 2, int off = 2) {
    for (int i = 0; i < w; i += on + off) g.hline(x + i, y, i + on <= w ? on : w - i, col);
}
inline void dashV(Gfx& g, int x, int y, int h, uint8_t col, int on = 2, int off = 2) {
    for (int i = 0; i < h; i += on + off) g.vline(x, y + i, i + on <= h ? on : h - i, col);
}
// Stipple under a unit curve: every third pixel, so a fill reads without a colour of its own.
template <class F>
void stipple(Gfx& g, Box b, F f) {
    for (int px = 1; px < b.w - 1; px += 3) {
        const int top = yUnit(b, f((float)px / (float)(b.w - 1)));
        for (int yy = b.y + b.h - 2; yy > top + 1; yy -= 3) g.pixel(b.x + px, yy, PBorder);
    }
}

// Faint grid: nx x ny cells (big pictures only).
inline void grid(const PicCtx& c, Box b, int nx, int ny) {
    if (c.small) return;
    for (int i = 1; i < nx; i++) dashV(c.g, b.x + b.w * i / nx, b.y, b.h, PBorder, 1, 2);
    for (int i = 1; i < ny; i++) dashH(c.g, b.x, b.y + b.h * i / ny, b.w, PBorder, 1, 2);
}
// Text only in the big picture.
inline void label(const PicCtx& c, const char* s, int x, int y, uint8_t col, Align a = Align::Left) {
    if (!c.small) c.g.text(s, x, y, col, a);
}
// The top line of a big picture: left and right captions.
inline void captions(const PicCtx& c, const char* left, uint8_t lcol, const char* right, uint8_t rcol) {
    if (c.small) return;
    if (left) c.g.text(left, c.x + 2, c.y + 2, lcol, Align::Left);
    if (right) c.g.text(right, c.x + c.w - 2, c.y + 2, rcol, Align::Right);
}
// The area under the captions.
inline Box body(const PicCtx& c) { return c.small ? inset(box(c), 1) : Box{c.x + 3, c.y + 10, c.w - 6, c.h - 13}; }

inline void fmt(const PicCtx& c, int i, char* out, int len, bool longForm = false) {
    paramFormat(c.m.type->params[i], c.m.params[i], out, len, longForm);
}
// Hz for captions: rates below 10 Hz keep two decimals (the param formatter rounds them).
inline void hzText(float hz, char* out, int len) {
    if (hz >= 1000.f) snprintf(out, (size_t)len, "%.2fKHZ", (double)(hz / 1000.f));
    else if (hz >= 10.f) snprintf(out, (size_t)len, "%dHZ", (int)lroundf(hz));
    else snprintf(out, (size_t)len, "%.2fHZ", (double)hz);
}
inline const char* enumName(const PicCtx& c, int i) {
    const ParamSpec& s = c.m.type->params[i];
    const int k = (int)lroundf(clampf(c.m.params[i], s.min, s.max) - s.min);
    return s.enumNames ? s.enumNames[k] : "";
}

// ---- painters (ui/pic_*.cpp) ----
using Painter = void (*)(const PicCtx&);
using GlyphPainter = bool (*)(Gfx&, const ModuleRec&, Box, uint8_t);
struct Entry {
    const char* id;
    Painter paint;
    GlyphPainter glyph;  // nullptr: the icon
};
extern const Entry kSourcePics[];
extern const int kNumSourcePics;
extern const Entry kModPics[];
extern const int kNumModPics;
extern const Entry kProcPics[];
extern const int kNumProcPics;
extern const Entry kSeqPics[];
extern const int kNumSeqPics;

}  // namespace pic
