#include <math.h>
#include <stdio.h>
#include <string.h>
#include "render_internal.h"

namespace rd {

namespace {

// Cable colours on `m` when a module is selected: its own cables, or those it shares with it.
int cableDots(const Patch& p, uint16_t selId, uint16_t modId, uint8_t* colors, int max) {
    int n = 0;
    for (int i = 0; i < p.cableCount() && n < max; i++) {
        const CableRec& c = p.cableAt(i);
        const bool touchesSel = c.fromId == selId || c.toId == selId;
        const bool touchesMod = c.fromId == modId || c.toId == modId;
        if (touchesSel && touchesMod) colors[n++] = c.color;
    }
    return n;
}

void drawDots(int col, int row, const uint8_t* colors, int n) {
    const int x = cellX(col), w = cellW(col), y = cellY(row) + cellH(row) - 5;
    const int total = n * 3 + (n - 1) * 2;
    int dx = x + (w - total) / 2;
    for (int i = 0; i < n; i++, dx += 5) cv.fillRect(dx, y, 3, 3, PCable0 + colors[i] % kNumCableColors);
}

// v1.4: a sequencer's step now as a tick riding its category bar; a clock's bar lit on the beat.
void stepTick(const ModuleRec& m, const Module& inst, int col, int row) {
    const char* id = m.type->id;
    const int x = cellX(col) + 2, y = cellY(row) + 1, w = cellW(col) - 4;
    if (!strcmp(id, "clk")) {
        if (inst.monitor(0) > 0.5f) cv.fillRect(x, y + 1, w, 2, PAccent);
        return;
    }
    const char* lenKey = !strcmp(id, "euc") ? "steps" : "len";
    if (strcmp(id, "seq") && strcmp(id, "trg") && strcmp(id, "euc") && strcmp(id, "kit") && strcmp(id, "rol") &&
        strcmp(id, "evo"))
        return;
    const int step = (int)inst.monitor(0);
    if (step < 0) return;  // KIT between passes
    int len = 12;
    for (int i = 0; i < m.type->numParams; i++)
        if (!strcmp(m.type->params[i].key, lenKey)) len = (int)lroundf(m.params[i]);
    len = len < 1 ? 1 : len;
    const int px = x + (step % len) * (w - 3) / (len > 1 ? len - 1 : 1);
    cv.fillRect(px, y, 3, 4, PAccent);
}

}  // namespace

void drawPatchView(const UiState& s, const Patch& p, Engine& e) {
    const ModuleRec* sel = s.selCell >= 0 ? p.findAt(s.page, (uint8_t)s.selCell) : nullptr;
    for (int row = 0; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            const int cell = row * kGridCols + col;
            const ModuleRec* m = p.findAt(s.page, (uint8_t)cell);
            const bool isSel = cell == s.selCell;
            if (!m) {
                emptyCell(col, row, keyLabel(row, col), isSel);
                continue;
            }
            int dy = 0;
            Module* inst = e.instance(m->id);
            if (inst && m->type->icon == Icon::Lfo) dy = (int)lroundf(sinf(inst->monitor(0) * 6.2831853f));
            // dim: waiting for RAM ("NO RAM"), silent; or bypassed (also slashed)
            moduleTile(*m->type, col, row, !inst || m->bypass, dy, m);
            if (inst && !m->bypass) stepTick(*m, *inst, col, row);
            if (m->bypass)
                cv.drawLine(cellX(col) + 3, cellY(row) + cellH(row) - 4, cellX(col) + cellW(col) - 4, cellY(row) + 4, PDim);
            if (inst && !m->bypass && m->type->numOuts > 0 && m->type->icon != Icon::Out) {  // activity: |out| peak
                float a = inst->peak() * (1.f / kAudioVolts);
                a = a > 1.f ? 1.f : a;
                const int h = (int)(a * (cellH(row) - 8) + 0.5f);
                if (h > 0) cv.fillRect(cellX(col) + cellW(col) - 3, cellY(row) + cellH(row) - 3 - h, 2, h, PAccent);
            }
            if (inst && !m->bypass && m->type->icon == Icon::Out) {
                float pk = inst->monitor(0);
                pk = pk < 0.f ? 0.f : (pk > 1.f ? 1.f : pk);
                const int h = (int)(pk * 20.f + 0.5f);
                if (h > 0) cv.fillRect(cellX(col) + 3, cellY(row) + 26 - h, 2, h, PCable0 + 5);
            }
            if (sel) {
                uint8_t colors[6];
                const int n = cableDots(p, sel->id, m->id, colors, 6);
                if (n) drawDots(col, row, colors, n);
            }
            if (s.carry.active && (gNow / 250) % 2 == 0) {  // modules that can take the cable pulse
                const bool fits = s.carry.fromOutput ? m->type->numIns > 0 : m->type->numOuts > 0;
                if (fits && m->id != s.carry.id) dotted(cellX(col), cellY(row), cellW(col), cellH(row), PAccent);
            }
            if (isSel) outline(col, row, PAccent);
            if (m->id == (uint16_t)s.moveFromId) dotted(cellX(col) + 1, cellY(row) + 1, cellW(col) - 2, cellH(row) - 2, PAccent);
        }
    }
}

}  // namespace rd
