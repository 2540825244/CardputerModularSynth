#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include "render_internal.h"
#include "ui/layout.h"

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
    r.c = moduleCell(*m.type, (uint8_t)(s.modCell / kGridCols), (uint8_t)(s.modCell % kGridCols));
    if (r.c.kind == CellKind::Param) {
        r.spec = &m.type->params[r.c.index];
        r.value = m.params[r.c.index];
    } else if (r.c.kind == CellKind::Out) {
        r.spec = &kAmountSpec;
        r.value = m.outAmount[r.c.index];
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
    const int filled = sel.spec ? (int)lroundf(paramToNorm(*sel.spec, sel.value) * 10.f) : 0;
    const int cur = sel.spec ? (int)lroundf(sel.value - sel.spec->min) : -1;
    for (int col = 0; col < kGridCols; col++) {
        const int x = cellX(col), y = cellY(0), w = cellW(col), h = cellH(0);
        bool on = false;
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
                on = sel.spec && col < filled;
                snprintf(lab, sizeof lab, "%s", keyLabel(0, col));
            }
        } else {
            snprintf(lab, sizeof lab, "%s", col == 10 ? "MIN" : "DEF");
        }
        cv.fillRect(x, y, w, h, on ? PAccent : PEmpty);
        smallText(lab, x + w / 2, y + h - 7, on ? PBg : labCol);
    }
    char name[16], val[16];
    if (sel.spec) {
        upper(sel.c.kind == CellKind::Out ? "AMT" : sel.spec->key, name, sizeof name);
        paramFormat(*sel.spec, sel.value, val, sizeof val, true);
    } else {
        upper(m.type->name, name, sizeof name);
        val[0] = 0;
    }
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PText);
    cv.setTextDatum(textdatum_t::top_left);
    cv.drawString(name, cellX(0) + 3, cellY(0) + 3);
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
    if (isOut) {
        char amt[8];
        snprintf(amt, sizeof amt, "%+d", (int)lroundf(m.outAmount[pin] * 100.f));
        smallText(amt, x + w / 2, y + 28, PDim);
    }
    if (target && pulse()) dotted(x, y, w, h, PAccent);
    if (selected) outline(col, row, PAccent);
}

// ---- live drawings ----

void drawFilterCurve(const ModuleRec& m, Module* inst, int x, int y, int w, int h) {
    float fc = inst ? inst->monitor(0) : m.params[0];
    if (fc < 20.f) fc = m.params[0];
    // DaisySP Svf: Q = 1 / (2 * (1 - res^0.25)), res = monitor(1)
    const float res = inst ? inst->monitor(1) : m.params[1];
    float q = 1.f / (2.f * (1.f - powf(res < 0.f ? 0.f : res, 0.25f)));
    q = q > 20.f ? 20.f : q;
    const bool hp = m.params[2] >= 0.5f;
    auto fx = [&](float f) { return x + (int)(log10f(f / 20.f) / 3.f * (w - 1)); };
    for (float f : {100.f, 1000.f, 10000.f}) cv.drawFastVLine(fx(f), y, h, PBorder);
    int prev = 0;
    for (int px = 0; px < w; px++) {
        const float f = 20.f * powf(1000.f, (float)px / (float)(w - 1));
        const float r = f / fc, r2 = r * r;
        const float d = 1.f - r2;
        float mag = 1.f / sqrtf(d * d + (r / q) * (r / q));
        if (hp) mag *= r2;
        const float db = 20.f * log10f(mag + 1e-6f);
        int yy = y + (int)((12.f - db) / 60.f * h);
        yy = yy < y ? y : (yy > y + h - 1 ? y + h - 1 : yy);
        if (px) cv.drawLine(x + px - 1, prev, x + px, yy, PAccent);
        prev = yy;
    }
    const int xc = fx(fc < 20.f ? 20.f : (fc > 20000.f ? 20000.f : fc));
    for (int j = 0; j < h; j += 4) cv.drawFastVLine(xc, y + j, 2, PAccent);
    smallText(hp ? "HP 12DB" : "LP 12DB", x + 18, y + h - 8, PDim);
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
    if (s.carry.active) drawBanner(s, p);
    else drawStrip(s, *m);
    if (t.drawing != Drawing::None) {
        const int x = cellX(5), y = cellY(1), w = cellX(11) + cellW(11) - x, h = cellY(3) + cellH(3) - y;
        cv.fillRect(x, y, w, h, PEmpty);
        Module* inst = e.instance(m->id);
        if (t.drawing == Drawing::FilterCurve) drawFilterCurve(*m, inst, x, y, w, h);
        else drawEnvelope(*m, inst, x, y, w, h);
    }
    for (int row = 1; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            const CellRef c = moduleCell(t, (uint8_t)row, (uint8_t)col);
            const bool selected = s.modCell == row * kGridCols + col;
            switch (c.kind) {
                case CellKind::Empty: emptyCell(col, row, keyLabel(row, col), selected); break;
                case CellKind::Param: paramCell(*m, col, row, c.index, selected); break;
                case CellKind::In: pinCell(s, p, *m, col, row, false, c.index, selected); break;
                case CellKind::Out: pinCell(s, p, *m, col, row, true, c.index, selected); break;
                default: break;
            }
        }
    }
    const Sel sel = selection(s, *m);
    Side4 sd = {};
    sd.left[0] = Ic::Back; sd.left[1] = Ic::Next; sd.left[2] = Ic::Cable; sd.left[3] = Ic::Minus;
    sd.leftIcon[0] = sd.leftIcon[1] = sd.leftIcon[2] = sd.leftIcon[3] = true;
    sd.right[0] = Ic::Trash; sd.right[1] = Ic::Undo; sd.right[2] = Ic::Toggle; sd.right[3] = Ic::Plus;
    sd.rightIcon[0] = sd.rightIcon[1] = sd.rightIcon[3] = true;
    // Enter = do-it on the selection: a pin carries, a stepped param toggles; hold it on a knob to tilt.
    if (sel.c.kind == CellKind::In || sel.c.kind == CellKind::Out) sd.right[2] = Ic::Cable;
    else if (sel.spec && sel.spec->curve == Curve::Stepped) sd.right[2] = Ic::Toggle;
    else sd.right[2] = Ic::Tilt;
    sd.rightIcon[2] = sel.c.kind != CellKind::Empty;
    if (s.view == View::KeysSettings) sd.left[0] = Ic::Play;  // esc flips back to the play page
    sd.fnActive = s.carry.active;
    drawSides(sd);
}

void drawKeysPlay(const UiState& s, const Patch& p) {
    static const char* const names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const ModuleRec* m = p.find(s.openId);
    for (int col = 0; col < kGridCols; col++) {
        char lab[8] = "";
        if (col == 0) snprintf(lab, sizeof lab, "OCT-");
        else if (col == 1) snprintf(lab, sizeof lab, "OCT+");
        else if (col == 2 && m) snprintf(lab, sizeof lab, "%+d", (int)m->params[0]);
        else if (col == 11) snprintf(lab, sizeof lab, "SET");
        emptyCell(col, 0, nullptr, false);
        if (lab[0]) smallText(lab, cellX(col) + cellW(col) / 2, cellY(0) + cellH(0) / 2 - 2, PText);
    }
    for (int row = 1; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            const int note = 48 + 12 * (3 - row) + col;
            char lab[8];
            snprintf(lab, sizeof lab, "%s%d", names[note % 12], note / 12 - 1);
            const bool held = (s.keysHeld >> ((3 - row) * 12 + col)) & 1;
            const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
            if (held) {
                cv.fillRect(x, y, w, h, PAccent);
                smallText(lab, x + w / 2, y + h / 2 - 2, PBg);
            } else {
                dotted(x, y, w, h, PBorder);
                smallText(lab, x + w / 2, y + h / 2 - 2, PText);
            }
        }
    }
    Side4 sd = {};
    sd.left[0] = Ic::Back;
    sd.leftIcon[0] = true;
    sd.right[1] = Ic::Undo;
    sd.rightIcon[1] = true;
    drawSides(sd);
}

}  // namespace rd
