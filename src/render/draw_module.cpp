#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include "render_internal.h"
#include "ui/anim.h"
#include "core/scales.h"
#include "ui/keys_layout.h"
#include "ui/layout.h"
#include "ui/views.h"

namespace rd {

namespace {

bool pulse() { return (gNow / 250) % 2 == 0; }  // 2 Hz

uint8_t cableCol(uint8_t c) { return PCable0 + c % kNumCableColors; }

struct PinInfo {
    int total;
    int n;
    uint8_t colors[3];
    const char* partner;
};

PinInfo pinInfo(const Patch& p, uint16_t id, bool isOut, uint8_t pin) {
    PinInfo r = {};
    for (int i = 0; i < p.cableCount(); i++) {
        const CableRec& c = p.cableAt(i);
        uint16_t other;
        if (isOut && c.fromId == id && c.fromOut == pin) other = c.toId;
        else if (!isOut && c.toId == id && c.toIn == pin) other = c.fromId;
        else continue;
        r.total++;
        if (r.n < 3) r.colors[r.n++] = c.color;
        if (!r.partner) {
            const ModuleRec* o = p.find(other);
            r.partner = o ? o->type->name : "?";
        }
    }
    return r;
}

void upper(const char* in, char* out, size_t n) {
    size_t i = 0;
    for (; in[i] && i + 1 < n; i++) out[i] = (char)toupper((unsigned char)in[i]);
    out[i] = 0;
}

struct Sel {
    CellRef c;
    const ParamSpec* spec;
    float value;
};

Sel selection(const UiState& s, const ModuleRec& m) {
    Sel r = {{CellKind::Empty, 0}, nullptr, 0.f};
    if (s.modCell < 0) return r;
    r.c = moduleCell(*m.type, s.modPage, (uint8_t)(s.modCell / kGridCols), (uint8_t)(s.modCell % kGridCols));
    if (r.c.kind == CellKind::Param) {
        r.spec = &m.type->params[r.c.index];
        r.value = m.params[r.c.index];
    } else if (r.c.kind == CellKind::Out) {
        r.spec = &kAmountSpec;
        r.value = m.outAmount[r.c.index];
    } else if (r.c.kind == CellKind::In) {
        r.spec = &kAmountSpec;
        r.value = m.inAmount[r.c.index];
    }
    return r;
}

void drawBanner(const UiState& s, const Patch& p) {
    const int x = cellX(0), y = cellY(0), w = kGridW - 1, h = cellH(0);
    cv.fillRect(x, y, w, h, PStrip);
    const int cx = x + 12, cy = y + h / 2;
    const uint8_t col = cableCol(p.nextCableColor());
    if (pulse()) cv.fillCircle(cx, cy, 4, col);
    else cv.drawCircle(cx, cy, 4, col);
    const ModuleRec* src = p.find(s.carry.id);
    char head[32] = "HOLDING";
    if (src) {
        const char* pin = s.carry.fromOutput ? (s.carry.pin < src->type->numOuts ? src->type->outLabels[s.carry.pin] : "?")
                                             : (s.carry.pin < src->type->numIns ? src->type->inLabels[s.carry.pin] : "?");
        snprintf(head, sizeof head, "HOLDING %s.%s", src->type->name, pin);
    }
    cv.setFont(&fonts::Font0);
    cv.setTextDatum(textdatum_t::middle_left);
    cv.setTextColor(PText);
    cv.drawString(head, x + 22, cy);
    const int hw = cv.textWidth(head);
    cv.setTextColor(PDim);
    cv.drawString(s.carry.fromOutput ? " -> FN+INPUT" : " -> FN+OUTPUT", x + 22 + hw, cy);
}

void drawStrip(const UiState& s, const ModuleRec& m) {
    const Sel sel = selection(s, m);
    const int n = sel.spec ? (int)(sel.spec->max - sel.spec->min) + 1 : 0;
    const bool steppedCells = sel.spec && sel.spec->curve == Curve::Stepped && n <= 10;
    // The fill eases to a new value (tau 60 ms); a new selection jumps.
    static float shown = 0.f;
    static int8_t shownCell = -1;
    static uint32_t lastMs = 0;
    const float target = sel.spec ? paramToNorm(*sel.spec, sel.value) : 0.f;
    if (shownCell != s.modCell) shown = target;
    else shown = ease(shown, target, (float)(gNow - lastMs), 60.f);
    shownCell = s.modCell;
    lastMs = gNow;
    const float norm = shown;
    const int cur = sel.spec ? (int)lroundf(sel.value - sel.spec->min) : -1;
    for (int col = 0; col < kGridCols; col++) {
        const int x = cellX(col), y = cellY(0), w = cellW(col), h = cellH(0);
        bool on = false;
        float fill = 0.f;  // knob segments: partial fill of the last one
        char lab[8] = "";
        uint8_t labCol = PFaint;
        if (col < 10) {
            if (steppedCells) {
                on = col == cur;
                if (col < n) {
                    if (sel.spec->enumNames) upper(sel.spec->enumNames[col], lab, sizeof lab);
                    else snprintf(lab, sizeof lab, "%d", (int)sel.spec->min + col);
                    labCol = PDim;
                }
            } else {
                fill = sel.spec ? stripFill(norm, col) : 0.f;
                on = fill >= 0.5f;
                snprintf(lab, sizeof lab, "%s", keyLabel(0, col));
            }
        } else {
            snprintf(lab, sizeof lab, "%s", col == 10 ? "MIN" : "DEF");
        }
        if (fill > 0.f && fill < 1.f) {
            cv.fillRect(x, y, w, h, PEmpty);
            cv.fillRect(x, y, (int)lroundf(w * fill), h, PAccent);
        } else {
            cv.fillRect(x, y, w, h, on ? PAccent : PEmpty);
        }
        smallText(lab, x + w / 2, y + h - 7, on ? PBg : labCol);
    }
    char name[16], val[16];
    if (sel.spec) {
        upper(sel.c.kind == CellKind::Param ? sel.spec->key : "AMT", name, sizeof name);
        paramFormat(*sel.spec, sel.value, val, sizeof val, true);
    } else {
        upper(m.type->name, name, sizeof name);
        val[0] = 0;
    }
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PText);
    cv.setTextDatum(textdatum_t::top_left);
    int nameX = cellX(0) + 3;
    if (m.bypass) {  // fn+del in the patch view toggles it: "BYP" leads the name, so it never
                     // overlaps it or the value (<= 11 + 8 characters of the 34 the strip holds)
        cv.setTextColor(PAccent);
        cv.drawString("BYP", nameX, cellY(0) + 3);
        cv.setTextColor(PText);
        nameX += 24;
    }
    cv.drawString(name, nameX, cellY(0) + 3);
    cv.setTextDatum(textdatum_t::top_right);
    cv.drawString(val, cellX(11) + cellW(11) - 3, cellY(0) + 3);
}

void paramCell(const ModuleRec& m, int col, int row, uint8_t idx, bool selected) {
    const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
    const ParamSpec& sp = m.type->params[idx];
    cv.fillRect(x, y, w, h, PEmpty);
    char val[8];
    paramFormat(sp, m.params[idx], val, sizeof val, false);
    smallText(sp.label, x + w / 2, y + h / 2 - 6, PText);
    smallText(val, x + w / 2, y + h / 2 + 2, PDim);
    if (sp.curve != Curve::Stepped) {  // value bar along the bottom
        const int bw = (int)lroundf((w - 4) * paramToNorm(sp, m.params[idx]));
        cv.drawFastHLine(x + 2, y + h - 4, w - 4, PBorder);
        if (bw > 0) cv.fillRect(x + 2, y + h - 5, bw, 2, PAccent);
    }
    if (selected) outline(col, row, PAccent);
}

void pinCell(const UiState& s, const Patch& p, const ModuleRec& m, int col, int row, bool isOut, uint8_t pin, bool selected) {
    const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
    cv.fillRect(x, y, w, h, PEmpty);
    const char* label = isOut ? m.type->outLabels[pin] : m.type->inLabels[pin];
    smallText(isOut ? "OUT" : "IN", x + w / 2, y + 2, PDim);
    smallText(label, x + w / 2, y + 8, PText);
    const bool target = s.carry.active && s.carry.fromOutput != isOut;
    const PinInfo pi = pinInfo(p, m.id, isOut, pin);
    const int cy = y + 17;
    if (pi.n) {
        const int total = pi.n * 3 + (pi.n - 1);
        int dx = x + (w - total) / 2;
        for (int i = 0; i < pi.n; i++, dx += 4) cv.fillRect(dx, cy - 1, 3, 3, cableCol(pi.colors[i]));
    } else {
        cv.drawCircle(x + w / 2, cy, 2, target ? cableCol(p.nextCableColor()) : PDim);
    }
    smallText(target && !pi.partner ? "?" : (pi.partner ? pi.partner : ""), x + w / 2, y + 22, PDim);
    {
        char amt[8];
        snprintf(amt, sizeof amt, "%+d", (int)lroundf((isOut ? m.outAmount[pin] : m.inAmount[pin]) * 100.f));
        smallText(amt, x + w / 2, y + 28, PDim);
    }
    if (target && pulse()) dotted(x, y, w, h, PAccent);
    if (selected) outline(col, row, PAccent);
}

// A sequencer step: filled when its bit is set, numbered 1..
void bitCell(const ModuleRec& m, int col, int row, CellRef c, bool selected) {
    const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
    const bool on = ((uint32_t)m.params[c.index] >> c.bit) & 1u;
    char lab[4];
    snprintf(lab, sizeof lab, "%d", c.bit + 1);
    if (on) {
        cv.fillRect(x + 1, y + 1, w - 2, h - 2, PText);
        smallText(lab, x + w / 2, y + h / 2 - 2, PBg);
    } else {
        dotted(x, y, w, h, PBorder);
        smallText(lab, x + w / 2, y + h / 2 - 2, PFaint);
    }
    if (selected) outline(col, row, PAccent);
}

// ---- live drawings ----

// SCO: the trace over the Drawing cells, a 0 V line, and the tuner's note and frequency.
void drawScope(Module* inst, int x, int y, int w, int h) {
    const int mid = y + h / 2;
    for (int i = 0; i < w; i += 3) cv.drawPixel(x + i, mid, PBorder);
    if (!inst) return;
    int n = 0, head = 0;
    const float* buf = inst->displayBuffer(&n, &head);
    if (!buf || n < 2) return;
    int prev = 0;
    for (int px = 0; px < w; px++) {
        const float v = buf[(head + px * (n - 1) / (w - 1)) % n];
        int yy = mid - (int)(v / 10.f * (h / 2 - 2));
        yy = yy < y ? y : (yy > y + h - 1 ? y + h - 1 : yy);
        if (px) cv.drawLine(x + px - 1, prev, x + px, yy, PAccent);
        prev = yy;
    }
    const float hz = inst->monitor(0);
    if (hz > 15.f && hz < 8000.f) {
        static const char* const kN[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const float midi = 69.f + 12.f * log2f(hz / 440.f);
        const int note = (int)lroundf(midi);
        const int cents = (int)lroundf((midi - (float)note) * 100.f);
        char lab[24];
        snprintf(lab, sizeof lab, "%s%d %+dC  %.1fHZ", kN[((note % 12) + 12) % 12], note / 12 - 1, cents, (double)hz);
        smallText(lab, x + w / 2, y + 2, PText);
    }
}

void drawFilterCurve(const ModuleRec& m, Module* inst, int x, int y, int w, int h) {
    float fc = inst ? inst->monitor(0) : m.params[0];
    if (fc < 20.f) fc = m.params[0];
    // DaisySP Svf: Q = 1 / (2 * (1 - res^0.25)), res = monitor(1)
    const float res = inst ? inst->monitor(1) : m.params[1];
    float q = 1.f / (2.f * (1.f - powf(res < 0.f ? 0.f : res, 0.25f)));
    q = q > 20.f ? 20.f : q;
    const int mode = (int)(m.params[2] + 0.5f);
    auto fx = [&](float f) { return x + (int)(log10f(f / 20.f) / 3.f * (w - 1)); };
    for (float f : {100.f, 1000.f, 10000.f}) cv.drawFastVLine(fx(f), y, h, PBorder);
    int prev = 0;
    for (int px = 0; px < w; px++) {
        const float f = 20.f * powf(1000.f, (float)px / (float)(w - 1));
        const float r = f / fc, r2 = r * r;
        const float d = 1.f - r2;
        float mag = 1.f / sqrtf(d * d + (r / q) * (r / q));
        if (mode == 1) mag *= r2;
        else if (mode == 2) mag *= r / q;
        else if (mode == 3) mag *= fabsf(d);
        const float db = 20.f * log10f(mag + 1e-6f);
        int yy = y + (int)((12.f - db) / 60.f * h);
        yy = yy < y ? y : (yy > y + h - 1 ? y + h - 1 : yy);
        if (px) cv.drawLine(x + px - 1, prev, x + px, yy, PAccent);
        prev = yy;
    }
    const int xc = fx(fc < 20.f ? 20.f : (fc > 20000.f ? 20000.f : fc));
    for (int j = 0; j < h; j += 4) cv.drawFastVLine(xc, y + j, 2, PAccent);
    static const char* const kModes[] = {"LP 12DB", "HP 12DB", "BP 12DB", "NOTCH"};
    smallText(kModes[mode < 0 ? 0 : (mode > 3 ? 3 : mode)], x + 18, y + h - 8, PDim);
}

void drawEnvelope(const ModuleRec& m, Module* inst, int x, int y, int w, int h) {
    const float a = m.params[0], d = m.params[1], sus = m.params[2], r = m.params[3];
    const float sum = a + d + r;
    const float flow = w * 0.75f;
    const int bottom = y + h - 6, top = y + 6;
    const int xa = x + (int)(flow * a / sum), xd = xa + (int)(flow * d / sum), xs = xd + (int)(w * 0.25f) - 2,
              xr = x + w - 2;
    const int ys = bottom - (int)(sus * (bottom - top));
    cv.drawLine(x, bottom, xa, top, PAccent);
    cv.drawLine(xa, top, xd, ys, PAccent);
    cv.drawLine(xd, ys, xs, ys, PAccent);
    cv.drawLine(xs, ys, xr, bottom, PAccent);
    cv.drawFastHLine(x, bottom, w, PBorder);
    if (!inst) return;
    const int stage = (int)inst->monitor(0);
    const float lvl = inst->monitor(1);
    if (stage < 1 || stage > 4) return;  // gate closed and idle
    int dx;
    if (stage == 1) dx = x + (int)((xa - x) * lvl);
    else if (stage == 2) dx = xa + (int)((xd - xa) * (sus < 1.f ? (1.f - lvl) / (1.f - sus) : 1.f));
    else if (stage == 3) dx = (xd + xs) / 2;
    else dx = xs + (int)((xr - xs) * (sus > 0.f ? 1.f - lvl / sus : 1.f));
    const int dyy = bottom - (int)(lvl * (bottom - top));
    cv.fillCircle(dx, dyy, 2, PText);
}

}  // namespace

void drawModuleView(const UiState& s, const Patch& p, Engine& e) {
    const ModuleRec* m = p.find(s.openId);
    if (!m) return;
    const ModuleTypeInfo& t = *m->type;
    if (modulePageKind(t, s.modPage) == PageKind::Play) {
        drawKeysPlay(s, p);
        return;
    }
    if (s.carry.active) drawBanner(s, p);
    else drawStrip(s, *m);
    const Drawing drawing = modulePageDrawing(t, s.modPage);
    if (drawing != Drawing::None && drawing != Drawing::Playhead) {
        const int x = cellX(5), y = cellY(1), w = cellX(11) + cellW(11) - x, h = cellY(3) + cellH(3) - y;
        cv.fillRect(x, y, w, h, PEmpty);
        Module* inst = e.instance(m->id);
        if (drawing == Drawing::FilterCurve) drawFilterCurve(*m, inst, x, y, w, h);
        else if (drawing == Drawing::Scope) drawScope(inst, x, y, w, h);
        else if (drawing == Drawing::Envelope) drawEnvelope(*m, inst, x, y, w, h);
    }
    for (int row = 1; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            const CellRef c = moduleCell(t, s.modPage, (uint8_t)row, (uint8_t)col);
            const bool selected = s.modCell == row * kGridCols + col;
            switch (c.kind) {
                case CellKind::Empty: emptyCell(col, row, keyLabel(row, col), selected); break;
                case CellKind::Param: paramCell(*m, col, row, c.index, selected); break;
                case CellKind::In: pinCell(s, p, *m, col, row, false, c.index, selected); break;
                case CellKind::Out: pinCell(s, p, *m, col, row, true, c.index, selected); break;
                case CellKind::Bit: bitCell(*m, col, row, c, selected); break;
                default: break;
            }
        }
    }
    if (drawing == Drawing::Playhead) {  // the playing step's column: a bar over its cells
        Module* inst = e.instance(m->id);
        const int step = inst ? (int)inst->monitor(0) : -1;
        if (step >= 0 && step < kGridCols)
            for (int row = 1; row < kGridRows; row++) cv.fillRect(cellX(step) + 1, cellY(row), cellW(step) - 2, 2, PAccent);
    }
}

void drawKeysPlay(const UiState& s, const Patch& p) {
    const ModuleRec* m = p.find(s.openId);
    if (!m) return;
    const int root = (int)m->params[6], layout = (int)m->params[4];
    for (int row = 0; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
            const int note = keysNoteFor(*m, row, col);
            if (note < 0) continue;  // a silent cell (piano gaps)
            const int n = note + 12 * (int)m->params[0];
            char lab[8];
            if (layout == kLayPad) snprintf(lab, sizeof lab, "%d", note - 35);
            else snprintf(lab, sizeof lab, "%s%d", kNoteNames[((n % 12) + 12) % 12], n / 12 - 1);
            const bool held = (s.keysHeld >> (row * kGridCols + col)) & 1;
            const bool isRoot = ((note - root) % 12 + 12) % 12 == 0;
            const bool black = layout != kLayPad && (0b010101001010 >> (((n % 12) + 12) % 12)) & 1;
            if (held) {
                cv.fillRect(x, y, w, h, PAccent);
                smallText(lab, x + w / 2, y + h / 2 - 2, PBg);
                continue;
            }
            if (isRoot) cv.fillRect(x, y, w, h, PEmpty);
            if (black) cv.fillRect(x + 2, y + 2, w - 4, 3, PDim);
            dotted(x, y, w, h, PBorder);
            smallText(lab, x + w / 2, y + h / 2 - 2, isRoot ? PText : PDim);
        }
    }
    smallText(keysLayoutName(layout), cellX(0) + cellW(0) / 2, cellY(0) + 2, PFaint);
}

}  // namespace rd
