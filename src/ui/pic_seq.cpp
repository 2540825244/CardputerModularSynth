// Pictures: sequencers, routing, utilities, OUT, GRD.
#include <cstdio>
#include <initializer_list>
#include "core/modules/common.h"
#include "core/modules/mod.h"
#include "core/scales.h"
#include "pictures_internal.h"

namespace pic {
namespace {

// ---- a one-octave piano from C: mask = in-scale semitones from C, root marked, held lit ----
void piano(Gfx& g, Box b, unsigned maskC, int root, int held, uint8_t accent) {
    static const int kWhite[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int kBlack[5] = {1, 3, 6, 8, 10};
    static const int kBlackAfter[5] = {0, 1, 3, 4, 5};  // the white key each black sits after
    const int ww = b.w / 7;
    for (int i = 0; i < 7; i++) {
        const int x = b.x + i * ww, n = kWhite[i];
        if (n == held) g.fillRect(x + 1, b.y + 1, ww - 1, b.h - 2, accent);
        g.rect(x, b.y, ww + 1, b.h, PDim);
        if ((maskC >> n) & 1u) {
            const int r = ww > 10 ? 2 : 1, cy = b.y + b.h - 3 - r;
            if (n == root) g.fillCircle(x + ww / 2, cy, r + 1, PText);
            else g.fillCircle(x + ww / 2, cy, r, n == held ? PBg : accent);
        }
    }
    const int bw = ww * 6 / 10 > 2 ? ww * 6 / 10 : 2, bh = b.h * 6 / 10;
    for (int i = 0; i < 5; i++) {
        const int n = kBlack[i], x = b.x + (kBlackAfter[i] + 1) * ww - bw / 2;
        g.fillRect(x, b.y, bw, bh, n == held ? accent : PDim);
        if ((maskC >> n) & 1u) {
            if (n == root) g.fillRect(x + 1, b.y + bh - 4, bw - 2, 3, PBg);
            else g.fillRect(x + 1, b.y + bh - 3, bw - 2, 2, n == held ? PBg : accent);
        }
    }
}
unsigned rotMask(unsigned mask, int root) {
    root = ((root % 12) + 12) % 12;
    return ((mask << root) | (mask >> (12 - root))) & 0xFFFu;
}

// QNT: the scale on the keys, from ROOT; TRN as text
void qnt(const PicCtx& c) {
    const int scale = ipar(c, 0), root = ipar(c, 1);
    const Box b = c.small ? body(c) : band(body(c), 0.12f, 0.8f);
    piano(c.g, b, rotMask(scaleMask(scale), root), root, -1, c.sel <= 1 ? PAccent : PText);
    if (c.small) return;
    char t[16];
    snprintf(t, sizeof t, "TRN %+d", ipar(c, 2));
    captions(c, enumName(c, 0), isSel(c, 0) ? PAccent : PDim, kNoteNames[((root % 12) + 12) % 12], isSel(c, 1) ? PAccent : PText);
    label(c, t, c.x + c.w - 3, c.y + c.h - 8, isSel(c, 2) ? PAccent : PDim, Align::Right);
    label(c, "ROOT", c.x + 2, c.y + c.h - 8, PDim);
}
// KEYS (SET page): the scale and root the layouts use, the held note (live), the layout
enum { kKOct, kKGlide, kKVel, kKLeg, kKLay, kKScl, kKRoot, kKLatch };
void keys(const PicCtx& c) {
    const int scale = ipar(c, kKScl), root = ipar(c, kKRoot);
    const int note = live(c) && mon(c, 1, 0.f) > 0.5f ? (((int)lroundf(mon(c, 0, 0.f)) % 12) + 12) % 12 : -1;
    const Box b = c.small ? body(c) : band(body(c), 0.12f, 0.7f);
    piano(c.g, b, rotMask(scaleMask(scale), root), root, note, c.sel == kKScl || c.sel == kKRoot || c.sel < 0 ? PAccent : PText);
    if (c.small) return;
    char l[24];
    snprintf(l, sizeof l, "%s  OCT %+d", enumName(c, kKLay), ipar(c, kKOct));
    captions(c, enumName(c, kKScl), isSel(c, kKScl) ? PAccent : PDim, kNoteNames[((root % 12) + 12) % 12], isSel(c, kKRoot) ? PAccent : PText);
    label(c, l, c.x + 2, c.y + c.h - 18, isSel(c, kKLay) || isSel(c, kKOct) ? PAccent : PText);
    char f[28];
    snprintf(f, sizeof f, "%s%s%s", par(c, kKLatch) >= 0.5f ? "LATCH " : "", par(c, kKLeg) >= 0.5f ? "LEGATO " : "",
             par(c, kKGlide) > 0.5f ? "GLIDE" : "");
    label(c, f, c.x + 2, c.y + c.h - 9, PDim);
}
// SEQ (SET page): the scale the steps snap to (root C), LEN and gate length
void seqSet(const PicCtx& c) {
    const Box b = c.small ? body(c) : band(body(c), 0.12f, 0.7f);
    piano(c.g, b, scaleMask(ipar(c, 15)), 0, -1, isSel(c, 15) || c.sel < 0 ? PAccent : PText);
    if (c.small) return;
    char l[24], r[16];
    snprintf(l, sizeof l, "%d STEPS", ipar(c, 13));
    snprintf(r, sizeof r, "GATE %d%%", (int)lroundf(par(c, 14) * 100.f));
    captions(c, enumName(c, 15), isSel(c, 15) ? PAccent : PDim, "ROOT C", PDim);
    label(c, l, c.x + 2, c.y + c.h - 9, isSel(c, 13) ? PAccent : PText);
    label(c, r, c.x + c.w - 3, c.y + c.h - 9, isSel(c, 14) ? PAccent : PText, Align::Right);
}
void seq(const PicCtx& c) {
    if (c.page == 1 || c.small) seqSet(c);
}

// ---- EUC: the ring: hits filled, rests hollow, the start tick, the step now (live) ----
void euc(const PicCtx& c) {
    const int steps = ipar(c, 0) < 1 ? 1 : (ipar(c, 0) > 16 ? 16 : ipar(c, 0)), fills = ipar(c, 1), rot = ipar(c, 2);
    const Box b = body(c);
    const int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    const int rad = (b.w < b.h ? b.w : b.h) / 2 - (c.small ? 3 : 7);
    const int dot = c.small ? 1 : (steps > 12 ? 3 : 4);
    if (!c.small) {
        c.g.circle(cx, cy, rad, PBorder);
        c.g.vline(cx, cy - rad - 7, 4, PText);
    }
    const int now = live(c) ? (int)mon(c, 0, -1.f) : -1;
    for (int i = 0; i < steps; i++) {
        const float a = -1.5707963f + 6.2831853f * (float)i / (float)steps;
        const int x = cx + (int)lroundf((float)rad * cosf(a)), y = cy + (int)lroundf((float)rad * sinf(a));
        const bool hit = euclidHit(steps, fills, rot, i);
        if (!c.small && i == now) c.g.circle(x, y, dot + 2, PText);
        if (hit) c.g.fillCircle(x, y, dot, c.sel < 0 || c.sel <= 2 ? PAccent : PText);
        else if (c.small) c.g.pixel(x, y, PDim);
        else c.g.circle(x, y, dot, PDim);
    }
    if (c.small) return;
    char n[8], d[12];
    snprintf(n, sizeof n, "%d", fills > steps ? steps : fills);
    snprintf(d, sizeof d, "OF %d", steps);
    c.g.text(n, cx, cy - 6, isSel(c, 1) ? PAccent : PText, Align::Center);
    c.g.text(d, cx, cy + 1, isSel(c, 0) ? PAccent : PDim, Align::Center);
    if (rot) {
        char r[12];
        snprintf(r, sizeof r, "ROT %d", rot);
        label(c, r, c.x + c.w - 3, c.y + c.h - 8, isSel(c, 2) ? PAccent : PDim, Align::Right);
    }
}
bool eucGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    const int steps = (int)lroundf(m.params[0]) < 1 ? 1 : ((int)lroundf(m.params[0]) > 16 ? 16 : (int)lroundf(m.params[0]));
    const int cx = b.x + b.w / 2, cy = b.y + b.h / 2, rad = (b.w < b.h ? b.w : b.h) / 2 - 1;
    for (int i = 0; i < steps; i++) {
        const float a = -1.5707963f + 6.2831853f * (float)i / (float)steps;
        const int x = cx + (int)lroundf((float)rad * cosf(a)), y = cy + (int)lroundf((float)rad * sinf(a));
        if (euclidHit(steps, (int)lroundf(m.params[1]), (int)lroundf(m.params[2]), i)) g.fillRect(x, y, 2, 2, col);
        else g.pixel(x, y, col);
    }
    return true;
}

// ---- TUR: the shift register (live; an example when idle), the loop, PRB and the CV ----
void tur(const PicCtx& c) {
    const int len = ipar(c, 0) < 2 ? 2 : (ipar(c, 0) > 16 ? 16 : ipar(c, 0));
    const uint32_t reg = live(c) ? (uint32_t)mon(c, 0, 0.f) : 0xA5C3u;
    const Box b = body(c);
    const Box row = c.small ? band(b, 0.2f, 0.8f) : band(b, 0.2f, 0.45f);
    const int bw = row.w / 16;
    for (int i = 0; i < len; i++) {
        const int x = row.x + i * bw;
        const bool on = (reg >> i) & 1u;
        if (on) c.g.fillRect(x + 1, row.y, bw - 2 > 0 ? bw - 2 : 1, row.h, i < 8 ? PAccent : PText);
        else c.g.rect(x + 1, row.y, bw - 2 > 0 ? bw - 2 : 1, row.h, PDim);
    }
    if (c.small) return;
    // the loop: the last bit feeds the first, flipped with PRB
    const int x0 = row.x + bw / 2, x1 = row.x + (len - 1) * bw + bw / 2, ly = row.y + row.h + 5;
    c.g.vline(x1, row.y + row.h, 5, PDim);
    c.g.hline(x0, ly, x1 - x0 + 1, PDim);
    c.g.vline(x0, row.y + row.h, 5, PDim);
    char p[16];
    snprintf(p, sizeof p, "FLIP %d%%", (int)lroundf(par(c, 1) * 100.f));
    label(c, p, x0, ly + 2, isSel(c, 1) ? PAccent : PText);
    // CV = the low 8 bits x RNG
    const float cv = (float)(reg & 0xFFu) / 255.f * par(c, 2);
    const Box m = {b.x, b.y + b.h - 6, b.w, 5};
    c.g.rect(m.x, m.y, m.w, m.h, PBorder);
    const int fw = (int)lroundf(clampf(cv / 5.f, 0.f, 1.f) * (m.w - 2));
    if (fw > 0) c.g.fillRect(m.x + 1, m.y + 1, fw, m.h - 2, isSel(c, 2) ? PAccent : PText);
    char l[16], r[16];
    snprintf(l, sizeof l, "%d BITS", len);
    snprintf(r, sizeof r, "CV %.2fV", (double)cv);
    captions(c, l, isSel(c, 0) ? PAccent : PDim, r, PText);
}

// ---- CLK: 8 beats of OUT, the DIV lane under it, BPM and the beat time; the beat lamp (live) ----
void clk(const PicCtx& c) {
    static const int kDivs[] = {1, 2, 3, 4, 6, 8, 16};
    const int di = ipar(c, 1) < 0 ? 0 : (ipar(c, 1) > 6 ? 6 : ipar(c, 1));
    const int div = kDivs[di], beats = div > 8 ? 16 : 8;
    const Box b = body(c);
    const Box l1 = c.small ? band(b, 0.f, 0.45f) : band(b, 0.1f, 0.45f), l2 = c.small ? band(b, 0.55f, 1.f) : band(b, 0.55f, 0.9f);
    for (int i = 0; i < beats; i++) {
        const int x = b.x + i * (b.w - 3) / beats;
        c.g.fillRect(x, l1.y, 2, l1.h, c.sel <= 0 ? PAccent : PText);
        if (i % div == 0) c.g.fillRect(x, l2.y, 2, l2.h, isSel(c, 1) ? PAccent : PText);
    }
    if (c.small) return;
    if (live(c) && mon(c, 0, 0.f) > 0.5f) c.g.fillCircle(c.x + c.w - 6, c.y + c.h - 6, 3, PAccent);
    else c.g.circle(c.x + c.w - 6, c.y + c.h - 6, 3, PDim);
    char l[16], r[16];
    snprintf(l, sizeof l, "%d BPM", (int)lroundf(par(c, 0)));
    snprintf(r, sizeof r, "%dMS", (int)lroundf(60000.f / clampf(par(c, 0), 20.f, 300.f)));
    captions(c, l, isSel(c, 0) ? PAccent : PText, r, PDim);
    label(c, "OUT", b.x, l1.y + l1.h + 1, PDim);
    label(c, enumName(c, 1), b.x, l2.y + l2.h + 1, isSel(c, 1) ? PAccent : PDim);
}

// ---- MUX / DMX: three wires and one, the chosen path bold (live when CV picks it) ----
void route(const PicCtx& c, bool demux) {
    const int pos = live(c) ? (int)mon(c, 0, 0.f) : ipar(c, 0);
    const Box b = body(c);
    const int xl = b.x + (c.small ? 2 : 12), xr = b.x + b.w - (c.small ? 3 : 12), xm = b.x + b.w / 2;
    const int ym = b.y + b.h / 2;
    static const char* const kN[] = {"A", "B", "C"};
    for (int k = 0; k < 3; k++) {
        const int y = b.y + b.h * (k + 1) / 4;
        const int xs = demux ? xr : xl;
        const bool on = k == pos;
        const uint8_t col = on ? (c.sel <= 0 ? PAccent : PText) : PBorder;
        c.g.line(xs, y, xm, ym, col);
        if (on) c.g.line(xs, y + 1, xm, ym + 1, col);
        c.g.fillCircle(xs, y, c.small ? 1 : 2, on ? col : PDim);
        if (!c.small) c.g.text(kN[k], demux ? xs + 5 : xs - 8, y - 2, on ? PText : PDim);
    }
    const int xo = demux ? xl : xr;
    c.g.line(xm, ym, xo, ym, c.sel <= 0 ? PAccent : PText);
    c.g.fillCircle(xo, ym, c.small ? 1 : 2, PText);
    c.g.fillCircle(xm, ym, c.small ? 1 : 3, PText);
    if (c.small) return;
    c.g.text(demux ? "IN" : "OUT", demux ? xo - 11 : xo + 4, ym - 2, PDim);
    captions(c, enumName(c, 1), isSel(c, 1) ? PAccent : PDim, kN[pos < 0 ? 0 : (pos > 2 ? 2 : pos)], PText);
}
void mux(const PicCtx& c) { route(c, false); }
void dmx(const PicCtx& c) { route(c, true); }

// ---- BRN: the coin: IN splits to A (PRB) and B, arm widths by chance ----
void brn(const PicCtx& c) {
    const float p = clampf(par(c, 0), 0.f, 1.f);
    const Box b = body(c);
    const int xl = b.x + (c.small ? 2 : 10), xr = b.x + b.w - (c.small ? 3 : 14), ym = b.y + b.h / 2;
    const int ya = b.y + b.h / 4, yb = b.y + b.h * 3 / 4, xm = b.x + b.w / 3;
    c.g.line(xl, ym, xm, ym, PText);
    const int ta = 1 + (int)lroundf(p * (c.small ? 1.f : 4.f)), tb = 1 + (int)lroundf((1.f - p) * (c.small ? 1.f : 4.f));
    for (int t = 0; t < ta; t++) c.g.line(xm, ym + t - ta / 2, xr, ya + t - ta / 2, c.sel <= 0 ? PAccent : PText);
    for (int t = 0; t < tb; t++) c.g.line(xm, ym + t - tb / 2, xr, yb + t - tb / 2, PDim);
    if (c.small) return;
    char a[12], bb[12];
    snprintf(a, sizeof a, "A %d%%", (int)lroundf(p * 100.f));
    snprintf(bb, sizeof bb, "B %d%%", (int)lroundf((1.f - p) * 100.f));
    c.g.text(a, xr + 2, ya - 8, c.sel <= 0 ? PAccent : PText, Align::Right);
    c.g.text(bb, xr + 2, yb + 4, PDim, Align::Right);
    captions(c, "IN", PDim, "COIN TOSS", PDim);
}

// ---- LGC: the truth as a timing diagram ----
void lgc(const PicCtx& c) {
    const Box b = body(c);
    static const char* const kN[] = {"A", "B", "AND", "OR", "XOR", "!A"};
    const int rows = 6, rh = b.h / rows, slots = 8, lx = c.small ? 0 : 14;
    for (int r = 0; r < rows; r++) {
        const Box rb = {b.x + lx, b.y + r * rh + 1, b.w - lx, rh - 2};
        if (!c.small) c.g.text(kN[r], b.x, rb.y + rb.h / 2 - 2, r < 2 ? PDim : PText);
        int prevY = -1;
        for (int s = 0; s < slots; s++) {
            const int a = (s / 2) % 2 == 0, bb = s % 2 == 0;
            const int v = r == 0 ? a : r == 1 ? bb : r == 2 ? (a & bb) : r == 3 ? (a | bb) : r == 4 ? (a ^ bb) : !a;
            const int x0 = rb.x + s * rb.w / slots, x1 = rb.x + (s + 1) * rb.w / slots - 1;
            const int y = v ? rb.y : rb.y + rb.h - 1;
            if (prevY >= 0 && prevY != y) c.g.vline(x0, rb.y, rb.h, r < 2 ? PDim : PAccent);
            c.g.hline(x0, y, x1 - x0 + 1, r < 2 ? PDim : PAccent);
            prevY = y;
        }
    }
}

// ---- level bars: MIX (4 levels and their sum against 1), VCA (LVL and the gain now), KNB ----
void mix(const PicCtx& c) {
    const Box b = body(c);
    const int n = 5, bw = b.w / n;
    float sum = 0.f;
    for (int i = 0; i < 4; i++) sum += clampf(par(c, i), 0.f, 1.f);
    const Box bars = c.small ? b : band(b, 0.f, 0.86f);
    const float top = 2.f;  // the sum's scale: 0..2 (4 inputs at full can reach 4)
    for (int i = 0; i < n; i++) {
        const float v = i < 4 ? clampf(par(c, i), 0.f, 1.f) / top : clampf(sum, 0.f, top) / top;
        const int h = (int)lroundf(v * (bars.h - 1));
        const uint8_t col = i < 4 ? (isSel(c, i) || c.sel < 0 ? PAccent : PText) : (sum > 1.f ? PAccent : PText);
        if (h > 0) c.g.fillRect(bars.x + i * bw + 2, bars.y + bars.h - h, bw - 4, h, col);
    }
    dashH(c.g, bars.x, bars.y + bars.h - (bars.h - 1) / 2, bars.w, PDim);  // 1.0
    if (c.small) return;
    static const char* const kN[] = {"L1", "L2", "L3", "L4", "SUM"};
    for (int i = 0; i < n; i++) c.g.text(kN[i], bars.x + i * bw + bw / 2, b.y + b.h - 6, PDim, Align::Center);
    char r[16];
    snprintf(r, sizeof r, "SUM %.2f", (double)sum);
    captions(c, "1.0 - -", PDim, r, sum > 1.f ? PAccent : PText);
}
void vca(const PicCtx& c) {
    const Box b = body(c);
    const float lvl = clampf(par(c, 0), 0.f, 1.f);
    const Box t = c.small ? b : cols(b, 0.f, 0.7f);
    // the amplifier: a triangle whose height is LVL
    const int x0 = t.x + 2, x1 = t.x + t.w - 3, ym = t.y + t.h / 2, hh = (int)lroundf(lvl * (t.h / 2 - 2));
    c.g.line(x0, ym - hh, x1, ym, c.sel <= 0 ? PAccent : PText);
    c.g.line(x0, ym + hh, x1, ym, c.sel <= 0 ? PAccent : PText);
    c.g.vline(x0, ym - hh, 2 * hh + 1, c.sel <= 0 ? PAccent : PText);
    if (c.small) return;
    const Box m = {b.x + b.w - 14, b.y + 2, 8, b.h - 4};
    c.g.rect(m.x, m.y, m.w, m.h, PBorder);
    const float g = clampf(mon(c, 0, 0.f), 0.f, 1.f);  // the gain now (live)
    const int gh = (int)lroundf(g * (m.h - 2));
    if (gh > 0) c.g.fillRect(m.x + 1, m.y + m.h - 1 - gh, m.w - 2, gh, PText);
    char l[16];
    snprintf(l, sizeof l, "LVL %d%%", (int)lroundf(lvl * 100.f));
    captions(c, l, isSel(c, 0) ? PAccent : PDim, nullptr, PDim);
}
void knb(const PicCtx& c) {
    const Box b = body(c);
    const Box bars = c.small ? b : band(b, 0.f, 0.86f);
    const int bw = bars.w / 4, ym = bars.y + bars.h / 2;
    c.g.hline(bars.x, ym, bars.w, PBorder);
    for (int i = 0; i < 4; i++) {
        const float v = clampf(par(c, i) / 5.f, -1.f, 1.f);
        const int h = (int)lroundf(fabsf(v) * (bars.h / 2 - 1));
        const uint8_t col = isSel(c, i) || c.sel < 0 ? PAccent : PText;
        if (h > 0) c.g.fillRect(bars.x + i * bw + 3, v >= 0.f ? ym - h : ym + 1, bw - 6, h, col);
        if (!c.small) {
            char k[12];
            snprintf(k, sizeof k, "%+.1f", (double)par(c, i));
            c.g.text(k, bars.x + i * bw + bw / 2, b.y + b.h - 6, isSel(c, i) ? PAccent : PDim, Align::Center);
        }
    }
}

// ---- OUT: the peak meter (live) against the -3 dBFS the presets aim at, the clip lamp,
// where it goes ----
void out(const PicCtx& c) {
    const Box b = body(c);
    const float pk = clampf(mon(c, 0, 0.f), 0.f, 1.f);
    const float db = pk > 1e-5f ? 20.f * log10f(pk) : -60.f;
    auto xOf = [&](Box m, float d) { return m.x + (int)lroundf(clampf((d + 30.f) / 30.f, 0.f, 1.f) * (m.w - 1)); };
    const Box m = c.small ? band(b, 0.3f, 0.7f) : Box{b.x, b.y + 22, b.w, 16};
    c.g.rect(m.x, m.y, m.w, m.h, PBorder);
    const int fx = xOf(m, db);
    if (db > -30.f) c.g.fillRect(m.x + 1, m.y + 1, fx - m.x - 1, m.h - 2, db > -1.f ? PAccent : PText);
    const int tx = xOf(m, -3.f);
    if (c.small) {
        c.g.vline(tx, m.y - 2, m.h + 4, PAccent);
        return;
    }
    for (float d : {-24.f, -18.f, -12.f, -6.f, -3.f, 0.f}) {
        const int x = xOf(m, d);
        c.g.vline(x, m.y + m.h, 2, PDim);
        char l[6];
        snprintf(l, sizeof l, "%d", (int)d);
        if (d != -3.f) c.g.text(l, x, m.y + m.h + 3, PDim, Align::Center);
    }
    c.g.fillRect(tx - 2, m.y - 5, 5, 2, PAccent);
    c.g.pixel(tx, m.y - 3, PAccent);
    c.g.text("TARGET -3", tx + 2, m.y - 11, PAccent, Align::Right);  // -3 sits at 90 %: the text goes left
    // the clip lamp: lit when the soft clip is working
    if (pk > 0.98f) c.g.fillCircle(b.x + b.w - 4, b.y + 4, 3, PAccent);
    else c.g.circle(b.x + b.w - 4, b.y + 4, 3, PDim);
    c.g.text("CLIP", b.x + b.w - 10, b.y + 2, PDim, Align::Right);
    char v[16];
    if (db > -59.f) snprintf(v, sizeof v, "%.1f DBFS", (double)db);
    else snprintf(v, sizeof v, "SILENT");
    c.g.text(v, b.x, b.y + 2, PText);
    // the destination and its volume
    const int line = live(c) ? (int)mon(c, 1, 0.f) : ipar(c, 2);
    const int jy = b.y + b.h - 12;
    char vol[24];
    snprintf(vol, sizeof vol, "%d%% %+.0fDB", (int)lroundf(par(c, line ? 1 : 0) * 100.f), (double)par(c, 3));
    if (line) {
        c.g.fillRect(b.x, jy, 22, 10, PText);
        c.g.text("LINE", b.x + 11, jy + 3, PBg, Align::Center);
        c.g.rect(b.x + 25, jy, 20, 10, PFaint);
        c.g.text("SPK", b.x + 35, jy + 3, PFaint, Align::Center);
    } else {
        c.g.rect(b.x, jy, 22, 10, PFaint);
        c.g.text("LINE", b.x + 11, jy + 3, PFaint, Align::Center);
        c.g.fillRect(b.x + 25, jy, 20, 10, PText);
        c.g.text("SPK", b.x + 35, jy + 3, PBg, Align::Center);
    }
    c.g.text(vol, b.x + b.w, jy + 3, c.sel >= 0 ? PAccent : PDim, Align::Right);
}

// ---- TLT: where the device is tilted (live roll / pitch, +-5 V), the 0 cross ----
void tilt(const PicCtx& c) {
    const Box b = body(c);
    const int s = b.w < b.h ? b.w : b.h;
    const Box sq = {b.x + (b.w - s) / 2, b.y + (b.h - s) / 2, s, s};
    c.g.rect(sq.x, sq.y, sq.w, sq.h, PBorder);
    if (!c.small) {
        dashH(c.g, sq.x, sq.y + sq.h / 2, sq.w, PBorder, 1, 2);
        dashV(c.g, sq.x + sq.w / 2, sq.y, sq.h, PBorder, 1, 2);
    }
    const float r = clampf(mon(c, 0, 0.f) / 5.f, -1.f, 1.f), p = clampf(mon(c, 1, 0.f) / 5.f, -1.f, 1.f);
    const int x = sq.x + (int)lroundf((r + 1.f) * 0.5f * (sq.w - 1)), y = sq.y + (int)lroundf((1.f - (p + 1.f) * 0.5f) * (sq.h - 1));
    c.g.fillCircle(x, y, c.small ? 1 : 3, PAccent);
    if (c.small) return;
    char l[16];
    snprintf(l, sizeof l, "+-%d DEG", ipar(c, 0));
    label(c, "ROLL", sq.x + sq.w + 2, sq.y + sq.h / 2 - 2, PDim);
    label(c, "PITCH", sq.x + sq.w / 2, sq.y + 1, PDim, Align::Center);
    label(c, l, c.x + 2, c.y + c.h - 8, isSel(c, 0) ? PAccent : PDim);
}

// ---- GRD: TONE = the engine and an XY pad of its two main knobs; VOICE = the ladder and the
// ADS; MOD = the LFO and where it goes ----
enum { kGEng, kGOct, kGTune, kGHarm, kGTimb, kGMorph, kGFma, kGTma, kGMma, kGDec, kGCol, kGVca, kGFlt, kGCut,
       kGRes, kGFeg, kGAtk, kGDcy, kGSus, kGLRate, kGLCut, kGLTim };
void grdTone(const PicCtx& c) {
    const int eng = ipar(c, kGEng);
    const Box b = body(c);
    const int s = c.small ? (b.w < b.h ? b.w : b.h) : b.h - 18;  // room under the pad for its x label
    const Box pad = {b.x + b.w - s, b.y + (c.small ? 0 : 10), s, s};
    c.g.rect(pad.x, pad.y, pad.w, pad.h, PBorder);
    if (!c.small) grid(c, pad, 4, 4);
    const float tx = clampf(par(c, kGTimb), 0.f, 1.f), my = clampf(par(c, kGMorph), 0.f, 1.f);
    const int r = c.small ? 1 : 2 + (int)lroundf(clampf(par(c, kGHarm), 0.f, 1.f) * 3.f);  // HAR: the dot's size
    const int px = pad.x + r + (int)lroundf(tx * (pad.w - 1 - 2 * r)), py = pad.y + r + (int)lroundf((1.f - my) * (pad.h - 1 - 2 * r));
    if (!c.small) {
        // where TMA / MMA push it (modulation amounts as arrows)
        const int ax = pad.x + 1 + (int)lroundf(clampf(tx + par(c, kGTma) * 0.5f, 0.f, 1.f) * (pad.w - 3));
        const int ay = pad.y + 1 + (int)lroundf((1.f - clampf(my + par(c, kGMma) * 0.5f, 0.f, 1.f)) * (pad.h - 3));
        if (ax != px) c.g.line(px, py, ax, py, isSel(c, kGTma) ? PAccent : PDim);
        if (ay != py) c.g.line(px, py, px, ay, isSel(c, kGMma) ? PAccent : PDim);
        dashV(c.g, px, py, pad.y + pad.h - 1 - py, isSel(c, kGTimb) ? PAccent : PFaint);
        dashH(c.g, pad.x, py, px - pad.x, isSel(c, kGMorph) ? PAccent : PFaint);
    }
    c.g.fillCircle(px, py, r, c.sel < 0 || c.sel == kGHarm || c.sel == kGTimb || c.sel == kGMorph ? PAccent : PText);
    if (c.small) return;
    c.g.text(grdEngineName(eng), b.x, b.y + 1, isSel(c, kGEng) ? PAccent : PText);
    const int lx = b.x, lw = pad.x - b.x - 3;
    (void)lw;
    for (int k = 0; k < 3; k++) {
        const int y = b.y + 16 + k * 17;
        const bool sel = c.sel == kGHarm + k;
        c.g.text(grdKnobLabel(eng, k), lx, y, sel ? PAccent : PText);
        char v[8];
        snprintf(v, sizeof v, "%d", (int)lroundf(par(c, kGHarm + k) * 100.f));
        c.g.text(v, lx + 18, y, sel ? PAccent : PDim);
        // the long name, cut to the space left of the pad
        char nm[12];
        snprintf(nm, sizeof nm, "%s", grdKnobName(eng, k));
        const int maxChars = (pad.x - lx - 2) / Gfx::kCharW;
        if (maxChars < (int)sizeof nm) nm[maxChars > 0 ? maxChars : 0] = 0;
        c.g.text(nm, lx, y + 7, sel ? PAccent : PFaint);
    }
    c.g.text(grdKnobLabel(eng, 1), pad.x + pad.w - 1, pad.y + pad.h + 1, isSel(c, kGTimb) ? PAccent : PDim, Align::Right);
    c.g.text(grdKnobLabel(eng, 2), pad.x + 2, pad.y + 2, isSel(c, kGMorph) ? PAccent : PDim);
}
void grdVoice(const PicCtx& c) {
    const Box b = body(c);
    const Box top = c.small ? band(b, 0.f, 0.5f) : band(b, 0.f, 0.48f), low = c.small ? band(b, 0.55f, 1.f) : band(b, 0.58f, 1.f);
    const int flt = ipar(c, kGFlt);
    const float fc = clampf(par(c, kGCut), 20.f, 18000.f), res = clampf(par(c, kGRes), 0.f, 0.97f);
    auto resp = [&](float u) {
        if (flt == 2) return 0.8f;
        const float f = 20.f * powf(1000.f, u), r = f / fc;
        float m = 1.f / sqrtf((1.f - r * r) * (1.f - r * r) + r * r / (0.5f + 4.f * res));
        m = flt == 1 ? m * r * r : m;
        m = m * m;  // 4 poles
        const float db = 20.f * log10f(m + 1e-6f);
        return clampf(1.f - (12.f - db) / 60.f, 0.f, 1.f);
    };
    traceUnit(c.g, top, resp, c.sel == kGFlt || c.sel == kGCut || c.sel == kGRes || c.sel < 0 ? PAccent : PText);
    // ADS: attack, decay to sustain, then release = decay (the Grind's ADS)
    const float a = sqrtf(par(c, kGAtk)), d = sqrtf(par(c, kGDcy)), sus = clampf(par(c, kGSus), 0.f, 1.f), sum = 2.f * d + a;
    const float xa = 0.75f * a / sum, xd = xa + 0.75f * d / sum, xs = xd + 0.25f;
    auto env = [&](float u) {
        if (u < xa) return xa > 0.f ? u / xa : 1.f;
        if (u < xd) return 1.f - (1.f - sus) * (u - xa) / (xd - xa);
        if (u < xs) return sus;
        return sus * (1.f - (u - xs) / (1.f - xs));
    };
    if (!c.small) stipple(c.g, low, env);
    traceUnit(c.g, low, env, c.sel == kGAtk || c.sel == kGDcy || c.sel == kGSus || c.sel < 0 ? PAccent : PText);
    if (c.small) return;
    char l[24];
    char hz[12];
    paramFormat(c.m.type->params[kGCut], fc, hz, sizeof hz, false);
    snprintf(l, sizeof l, "%s %s", enumName(c, kGFlt), hz);
    label(c, l, top.x, top.y + top.h + 1, c.sel == kGFlt || c.sel == kGCut ? PAccent : PDim);
    char f[16];
    snprintf(f, sizeof f, "FEG %+d", (int)lroundf(par(c, kGFeg) * 100.f));
    label(c, f, top.x + top.w, top.y + top.h + 1, isSel(c, kGFeg) ? PAccent : PDim, Align::Right);
    captions(c, nullptr, PDim, "ADS", PDim);
}
void grdMod(const PicCtx& c) {
    const Box b = body(c);
    const Box w = c.small ? b : band(b, 0.f, 0.6f);
    trace(c.g, w, [&](float u) { const float p = u * 2.f - floorf(u * 2.f); return (p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p) * 0.9f; },
          c.sel <= kGLRate ? PAccent : PText, isSel(c, kGLRate));
    if (c.small) return;
    static const char* const kN[] = {"> CUT", "> TIM"};
    for (int k = 0; k < 2; k++) {
        const Box r = {b.x + 26, b.y + b.h * 7 / 10 + k * 12, b.w - 26, 6};
        const int mid = r.x + r.w / 2;
        c.g.vline(mid, r.y - 1, r.h + 2, PBorder);
        const float v = clampf(par(c, kGLCut + k), -1.f, 1.f);
        const int len = (int)lroundf(fabsf(v) * (r.w / 2 - 1));
        if (len > 0) c.g.fillRect(v >= 0.f ? mid : mid - len, r.y, len, r.h, isSel(c, kGLCut + k) ? PAccent : PText);
        c.g.text(kN[k], b.x, r.y, PDim);
    }
    char r[16];
    hzText(par(c, kGLRate), r, sizeof r);
    captions(c, "TRI LFO", PDim, r, isSel(c, kGLRate) ? PAccent : PText);
}
void grd(const PicCtx& c) {
    if (c.page == 2) grdVoice(c);
    else if (c.page == 3) grdMod(c);
    else grdTone(c);
}

}  // namespace

const Entry kSeqPics[] = {
    {"qnt", qnt, nullptr}, {"keys", keys, nullptr}, {"seq", seq, nullptr}, {"euc", euc, eucGlyph},
    {"tur", tur, nullptr}, {"clk", clk, nullptr},   {"mux", mux, nullptr}, {"dmx", dmx, nullptr},
    {"brn", brn, nullptr}, {"lgc", lgc, nullptr},   {"mix", mix, nullptr}, {"vca", vca, nullptr},
    {"knb", knb, nullptr}, {"out", out, nullptr},   {"tilt", tilt, nullptr}, {"grd", grd, nullptr},
};
const int kNumSeqPics = sizeof kSeqPics / sizeof kSeqPics[0];

}  // namespace pic
