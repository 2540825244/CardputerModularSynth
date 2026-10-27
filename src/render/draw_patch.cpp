#include <math.h>
#include <stdio.h>
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
            moduleTile(*m->type, col, row, false, dy);
            if (inst && m->type->icon == Icon::Out) {
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
            if (isSel) outline(col, row, PAccent);
            if (m->id == (uint16_t)s.moveFromId) dotted(cellX(col) + 1, cellY(row) + 1, cellW(col) - 2, cellH(row) - 2, PAccent);
        }
    }
    Side4 sd = {};
    sd.left[0] = Ic::File; sd.left[2] = Ic::Plus; sd.left[3] = Ic::Move;
    sd.leftIcon[0] = sd.leftIcon[2] = sd.leftIcon[3] = true;
    sd.right[0] = Ic::Trash; sd.right[1] = Ic::Undo; sd.right[2] = Ic::Enter; sd.right[3] = Ic::Play;
    sd.rightIcon[0] = sd.rightIcon[1] = sd.rightIcon[2] = sd.rightIcon[3] = true;
    char page[8];
    snprintf(page, sizeof page, "%d/%d", s.page + 1, kPages);
    sd.tabText = page;
    sd.fnActive = s.carry.active;
    drawSides(sd);
}

void drawModulePlaceholder(const UiState& s, const Patch& p) {
    const ModuleRec* m = p.find(s.openId);
    Side4 sd = {};
    sd.left[0] = Ic::Back;
    sd.leftIcon[0] = true;
    sd.right[1] = Ic::Undo;
    sd.rightIcon[1] = true;
    sd.fnActive = s.carry.active;
    drawSides(sd);
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PText);
    cv.setTextDatum(textdatum_t::middle_center);
    cv.drawString(m ? m->type->name : "?", kGridX + kGridW / 2, 50);
    cv.drawString("MODULE VIEW", kGridX + kGridW / 2, 70);
}

}  // namespace rd
