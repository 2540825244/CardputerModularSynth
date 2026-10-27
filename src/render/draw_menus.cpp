#include <stdio.h>
#include <string.h>
#include "core/registry.h"
#include "render_internal.h"

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

void drawDetails(const ModuleTypeInfo& t, const Refusal& r) {
    const int x = kGridX + 4, w = kGridW - 8;
    cv.fillRect(cellX(0), cellY(2), kGridW - 1, cellY(4) - cellY(2) - 1, PEmpty);
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PText);
    cv.setTextDatum(textdatum_t::top_left);
    cv.drawString(t.name, x, cellY(2) + 4);
    smallText(t.blurb, x + w / 2, cellY(2) + 16, PDim);
    char in[24], out[24], line[48];
    joinLabels(in, sizeof in, t.inLabels, t.numIns);
    joinLabels(out, sizeof out, t.outLabels, t.numOuts);
    snprintf(line, sizeof line, "IN %s   OUT %s", in, out);
    smallText(line, x + w / 2, cellY(2) + 26, PText);
    if (r.ok) snprintf(line, sizeof line, "CPU %u CYC   RAM %u B", (unsigned)t.cpuCycles, (unsigned)t.ramBytes);
    else snprintf(line, sizeof line, "%s", r.reason);
    smallText(line, x + w / 2, cellY(2) + 38, r.ok ? PDim : PAccent);
    if (r.ok) drawIcon(Ic::Enter, cellX(11) + 2, cellY(3) + 10, PText);
}

}  // namespace

void drawPlaceView(const UiState& s, const Patch& p, Engine& e) {
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    for (int col = 0; col < kGridCols; col++) {
        if (col >= n) {
            emptyCell(col, 1, nullptr, false);
            continue;
        }
        const bool ok = e.canAdd(p, *types[col]).ok;
        moduleTile(*types[col], col, 1, !ok, 0);
        if (col == s.placeType) outline(col, 1, PAccent);
    }
    smallText("PLACE MODULE", kGridX + kGridW / 2, 6, PDim);
    if (s.placeType >= 0 && s.placeType < n) drawDetails(*types[s.placeType], e.canAdd(p, *types[s.placeType]));
    else smallText("PRESS A TYPE KEY", kGridX + kGridW / 2, cellY(2) + 12, PDim);
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
    }
    cv.setFont(&fonts::Font0);
    cv.setTextSize(2);
    cv.setTextColor(PText);
    cv.setTextDatum(textdatum_t::middle_center);
    cv.drawString(s.fileSaveMode ? "SAVE" : "LOAD", kGridX + kGridW / 2, cellY(2));
    cv.setTextSize(1);
    smallText(s.fileSaveMode ? "PRESS A SLOT TO SAVE" : "PRESS A SLOT TO LOAD", kGridX + kGridW / 2, cellY(3) - 6, PDim);
    smallText("TAB = SAVE / LOAD", kGridX + kGridW / 2, cellY(3) + 6, PDim);
    Side4 sd = {};
    sd.left[0] = Ic::Back;
    sd.left[1] = Ic::Toggle;
    sd.leftIcon[0] = sd.leftIcon[1] = true;
    sd.right[1] = Ic::Undo;
    sd.rightIcon[1] = true;
    drawSides(sd);
}

}  // namespace rd
