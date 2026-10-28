#include <stdio.h>
#include <string.h>
#include "core/registry.h"
#include "render_internal.h"
#include "ui/layout.h"

namespace rd {

namespace {

// Joins pin labels as "A B C"; "-" when there are none.
void joinLabels(char* out, size_t n, const char* const* labels, int count) {
    out[0] = 0;
    for (int i = 0; i < count; i++) {
        strncat(out, labels[i], n - strlen(out) - 1);
        if (i + 1 < count) strncat(out, " ", n - strlen(out) - 1);
    }
    if (!count) snprintf(out, n, "-");
}

// Details of the selected type in the top-row band (rows 1-3 hold the tiles).
void drawDetails(const ModuleTypeInfo& t, const Refusal& r) {
    const int cx = kGridX + kGridW / 2, y = cellY(0);
    cv.fillRect(cellX(0), y, kGridW - 1, cellH(0), PEmpty);
    char in[24], out[24], line[64];
    snprintf(line, sizeof line, "%s  %s", t.name, t.blurb);
    smallText(line, cx, y + 2, PText);
    joinLabels(in, sizeof in, t.inLabels, t.numIns);
    joinLabels(out, sizeof out, t.outLabels, t.numOuts);
    snprintf(line, sizeof line, "IN %s   OUT %s", in, out);
    smallText(line, cx, y + 10, PText);
    if (r.ok) snprintf(line, sizeof line, "CPU %u CYC   RAM %u B", (unsigned)t.cpuCycles, (unsigned)t.ramBytes);
    else snprintf(line, sizeof line, "%s", r.reason);
    smallText(line, cx, y + 18, r.ok ? PDim : PAccent);
    if (r.ok) smallText("ENTER OR HOLD TO PLACE", cx, y + 26, PDim);
}

}  // namespace

void drawPlaceView(const UiState& s, const Patch& p, Engine& e) {
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    for (int row = 1; row < kGridRows; row++)
        for (int col = 0; col < kGridCols; col++) {
            const int t = placeTypeAt((uint8_t)row, (uint8_t)col);
            if (t < 0) {
                emptyCell(col, row, nullptr, false);
                continue;
            }
            moduleTile(*types[t], col, row, !e.canAdd(p, *types[t]).ok, 0);
            if (t == s.placeType) outline(col, row, PAccent);
        }
    if (s.placeType >= 0 && s.placeType < n) {
        drawDetails(*types[s.placeType], e.canAdd(p, *types[s.placeType]));
    } else {
        smallText("PLACE MODULE", kGridX + kGridW / 2, cellY(0) + 6, PDim);
        smallText("TAP A TYPE TO SEE IT, HOLD OR FN+TAP TO PLACE", kGridX + kGridW / 2, cellY(0) + 18, PDim);
    }
    Side4 sd = {};
    sd.left[0] = Ic::Back;
    sd.leftIcon[0] = true;
    if (s.placeType >= 0) {
        sd.right[2] = Ic::Enter;
        sd.rightIcon[2] = true;
    }
    drawSides(sd);
}

void drawFileView(const UiState& s, const bool filled[12]) {
    for (int col = 0; col < kGridCols; col++) {
        char label[4];
        snprintf(label, sizeof label, "%d", col + 1);
        const int x = cellX(col), y = cellY(0), w = cellW(col), h = cellH(0);
        if (filled[col]) {
            cv.fillRect(x, y, w, h, PEmpty);
            cv.drawRect(x, y, w, h, PText);
            drawIcon(Ic::File, x + (w - 12) / 2, y + 6, PText);
            smallText(label, x + w / 2, y + 22, PText);
        } else {
            emptyCell(col, 0, label, false);
        }
        if (col == s.fileSlot) outline(col, 0, PAccent);
    }
    cv.setFont(&fonts::Font0);
    cv.setTextSize(2);
    cv.setTextColor(PText);
    cv.setTextDatum(textdatum_t::middle_center);
    cv.drawString(s.fileSaveMode ? "SAVE" : "LOAD", kGridX + kGridW / 2, cellY(2));
    cv.setTextSize(1);
    smallText(s.fileSaveMode ? "TAP A SLOT, ENTER TO SAVE" : "TAP A SLOT, ENTER TO LOAD", kGridX + kGridW / 2, cellY(3) - 6, PDim);
    smallText("TAB = SAVE / LOAD", kGridX + kGridW / 2, cellY(3) + 6, PDim);
    Side4 sd = {};
    sd.left[0] = Ic::Back;
    sd.left[1] = Ic::Toggle;
    sd.leftIcon[0] = sd.leftIcon[1] = true;
    sd.right[1] = Ic::Undo;
    sd.rightIcon[1] = true;
    sd.right[2] = Ic::Enter;
    sd.rightIcon[2] = s.fileSlot >= 0;
    drawSides(sd);
}

}  // namespace rd
