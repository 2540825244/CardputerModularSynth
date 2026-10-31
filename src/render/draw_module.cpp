#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "render_internal.h"
#include "ui/anim.h"
#include "core/scales.h"
#include "core/modules/seq2.h"
#include "ui/keys_layout.h"
#include "ui/layout.h"
#include "ui/views.h"
#include "ui/pictures.h"
#include "gfx_canvas.h"

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
    // v1.4: a knob that goes both ways (TUNE, AMT, OFS ...) fills from its zero
    const bool bipolar = sel.spec && sel.spec->curve == Curve::Lin && sel.spec->min < 0.f && sel.spec->max > 0.f;
    const float zero = bipolar ? paramToNorm(*sel.spec, 0.f) : 0.f;
    for (int col = 0; col < kGridCols; col++) {
        const int x = cellX(col), y = cellY(0), w = cellW(col), h = cellH(0);
        bool on = false;
        float fill = 0.f, fa = 0.f, fb = 0.f;  // knob segments: the filled part [fa, fb] of this one
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
                if (sel.spec) stripSpan(norm, zero, col, &fa, &fb);
                fill = fb > fa ? fb - fa : 0.f;
                on = fill >= 0.5f;
                snprintf(lab, sizeof lab, "%s", keyLabel(0, col));
            }
        } else {
            snprintf(lab, sizeof lab, "%s", col == 10 ? "MIN" : "DEF");
        }
        if (fill > 0.f && fill < 1.f) {
            cv.fillRect(x, y, w, h, PEmpty);
            cv.fillRect(x + (int)lroundf(w * fa), y, (int)lroundf(w * fill), h, PAccent);
        } else {
            cv.fillRect(x, y, w, h, on ? PAccent : PEmpty);
        }
        smallText(lab, x + w / 2, y + h - 7, on ? PBg : labCol);
    }
    if (sel.spec && !steppedCells) {  // the zero of a bipolar knob, and a tick at the default
        const int x0 = cellX(0), x10 = cellX(9) + cellW(9), yb = cellY(0) + cellH(0);
        if (bipolar) cv.drawFastVLine(x0 + (int)lroundf(zero * (x10 - x0)), cellY(0) + 10, cellH(0) - 12, PText);
        const int dx = x0 + (int)lroundf(paramToNorm(*sel.spec, sel.c.kind == CellKind::Param ? sel.spec->def : 1.f) * (x10 - x0));
        cv.fillTriangle(dx - 2, yb - 1, dx + 2, yb - 1, dx, yb - 4, PText);
    }
    char name[16], val[16];
    if (sel.spec) {
        if (sel.c.kind == CellKind::Param) paramName(m, sel.c.index, name, sizeof name);  // GRD: the engine's names
        else upper("AMT", name, sizeof name);
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

// stepBar: a sequencer step's pitch (a Playhead page): a vertical bar from the cell's middle, so a
// row of steps reads as the melody.
void paramCell(const ModuleRec& m, int col, int row, uint8_t idx, bool selected, bool stepBar) {
    const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
    const ParamSpec& sp = m.type->params[idx];
    cv.fillRect(x, y, w, h, PEmpty);
    char val[8];
    paramFormat(sp, m.params[idx], val, sizeof val, false);
    if (stepBar) {
        const int mid = y + h / 2 + 2, span = h / 2 - 6;
        const float span01 = sp.max > sp.min ? (m.params[idx] - (sp.min + sp.max) * 0.5f) / ((sp.max - sp.min) * 0.5f) : 0.f;
        const int bh = (int)lroundf((span01 < -1.f ? -1.f : (span01 > 1.f ? 1.f : span01)) * span);
        cv.drawFastHLine(x + 2, mid, w - 4, PBorder);
        if (bh > 0) cv.fillRect(x + 4, mid - bh, w - 8, bh, selected ? PAccent : PText);
        else if (bh < 0) cv.fillRect(x + 4, mid + 1, w - 8, -bh, selected ? PAccent : PText);
        smallText(val, x + w / 2, y + h - 7, selected ? PText : PDim);
        if (selected) outline(col, row, PAccent);
        return;
    }
    smallText(paramLabel(m, idx), x + w / 2, y + h / 2 - 6, PText);
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
        snprintf(lab, sizeof lab, "%s%d %+dC  %.1fHZ", kN[((note % 12) + 12) % 12], (note >= 0 ? note / 12 : (note - 11) / 12) - 1,
                 cents, (double)hz);
        smallText(lab, x + w / 2, y + 2, PText);
    }
}

}  // namespace

void drawModuleView(const UiState& s, const Patch& p, Engine& e) {
    const ModuleRec* m = p.find(s.openId);
    if (!m) return;
    const ModuleTypeInfo& t = *m->type;
    if (modulePageKind(t, s.modPage) == PageKind::Play) {
        if (&t == &kRolType) drawRollPage(canvasGfx(), *m, e.instance(m->id), s, kGridX, 0, kGridW, kH);
        else drawKeysPlay(s, p);
        return;
    }
    if (s.carry.active) drawBanner(s, p);
    else drawStrip(s, *m);
    const Drawing drawing = modulePageDrawing(t, s.modPage);
    if (drawing == Drawing::Picture || drawing == Drawing::Scope) {
        const int x = cellX(5), y = cellY(1), w = cellX(11) + cellW(11) - x, h = cellY(3) + cellH(3) - y;
        cv.fillRect(x, y, w, h, PEmpty);
        Module* inst = e.instance(m->id);
        if (drawing == Drawing::Scope) {
            drawScope(inst, x, y, w, h);
        } else {
            const Sel sel = selection(s, *m);
            const int param = sel.c.kind == CellKind::Param ? sel.c.index : -1;
            cv.setClipRect(x, y, w, h);
            drawPicture(PicCtx{canvasGfx(), *m, inst, s.modPage, param, x, y, w, h, false});
            cv.clearClipRect();
        }
    }
    for (int row = 1; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            const CellRef c = moduleCell(t, s.modPage, (uint8_t)row, (uint8_t)col);
            const bool selected = s.modCell == row * kGridCols + col;
            switch (c.kind) {
                case CellKind::Empty: emptyCell(col, row, keyLabel(row, col), selected); break;
                case CellKind::Param: paramCell(*m, col, row, c.index, selected, drawing == Drawing::Playhead); break;
                case CellKind::In: pinCell(s, p, *m, col, row, false, c.index, selected); break;
                case CellKind::Out: pinCell(s, p, *m, col, row, true, c.index, selected); break;
                case CellKind::Bit: bitCell(*m, col, row, c, selected); break;
                default: break;
            }
        }
    }
    if (drawing == Drawing::Playhead) {  // steps past LEN: hatched, they do not play
        int len = kGridCols;
        for (int i = 0; i < t.numParams; i++)
            if (!strcmp(t.params[i].key, "len")) len = (int)lroundf(m->params[i]);
        for (int col = len < 0 ? 0 : len; col < kGridCols; col++)
            for (int row = 1; row < kGridRows; row++) {
                const CellRef c = moduleCell(t, s.modPage, (uint8_t)row, (uint8_t)col);
                if (c.kind != CellKind::Bit && c.kind != CellKind::Param) continue;
                const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
                for (int k = -h; k < w; k += 4)  // diagonals, clipped to the cell
                    for (int j = 0; j < h; j++) {
                        const int xx = x + k + j;
                        if (xx >= x && xx < x + w && (j & 1) == 0) cv.drawPixel(xx, y + h - 1 - j, PBorder);
                    }
                if (s.modCell == row * kGridCols + col) outline(col, row, PAccent);  // over the hatch
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
