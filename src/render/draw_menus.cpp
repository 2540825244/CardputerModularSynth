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

// Details of the selected type in the top-row band (rows 1-3 hold the tiles), in Font0.
void drawDetails(const ModuleTypeInfo& t, const Refusal& r, uint32_t cpuUsed) {
    const int x = cellX(0), y = cellY(0), w = kGridW - 1;
    cv.fillRect(x, y, w, cellH(0), PEmpty);
    cv.setFont(&fonts::Font0);
    cv.setTextDatum(textdatum_t::top_left);
    cv.setTextColor(PText);
    cv.drawString(t.name, x + 3, y + 2);
    cv.setTextColor(PDim);
    cv.drawString(t.blurb, x + 25, y + 2);
    char in[24], out[24], line[64];
    joinLabels(in, sizeof in, t.inLabels, t.numIns > 4 ? 4 : t.numIns);
    joinLabels(out, sizeof out, t.outLabels, t.numOuts);
    snprintf(line, sizeof line, "IN %s%s  OUT %s", in, t.numIns > 4 ? ".." : "", out);
    cv.setTextColor(PText);
    cv.drawString(line, x + 3, y + 12);
    if (!r.ok) {
        cv.setTextColor(PAccent);
        cv.drawString(r.reason, x + 3, y + 22);
        return;
    }
    // CPU: what this type would take of the budget, on top of what is used.
    const int bw = 60, bx = x + 3, by = y + 24;
    cv.drawRect(bx, by, bw, 5, PDim);
    const int used = (int)(bw * (float)cpuUsed / (float)kCpuBudget);
    const int add = (int)(bw * (float)t.cpuCycles / (float)kCpuBudget) + 1;
    cv.fillRect(bx, by, used > bw ? bw : used, 5, PDim);
    cv.fillRect(bx + used, by, used + add > bw ? bw - used : add, 5, PAccent);
    smallText("CPU", bx + bw + 9, by, PDim);
    cv.setTextDatum(textdatum_t::top_right);
    cv.setTextColor(PDim);
    cv.drawString("ENTER=PLACE", x + w - 3, y + 22);
}

}  // namespace

void drawPlaceView(const UiState& s, const Patch& p, Engine& e) {
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    for (int row = 1; row < kGridRows; row++)
        for (int col = 0; col < kGridCols; col++) {
            const int t = placeTypeAt(s.placePage, (uint8_t)row, (uint8_t)col);
            if (t < 0) {
                emptyCell(col, row, nullptr, false);
                continue;
            }
            moduleTile(*types[t], col, row, !e.canAdd(p, *types[t]).ok, 0);
            if (t == s.placeType) outline(col, row, PAccent);
        }
    if (s.placeType >= 0 && s.placeType < n) {
        drawDetails(*types[s.placeType], e.canAdd(p, *types[s.placeType]), e.estimatedCycles());
    } else {
        static const char* const kTitles[] = {"SOURCES", "MODULATORS", "PROCESSORS", "EFFECTS", "UTILITIES"};
        cv.setFont(&fonts::Font0);
        cv.setTextColor(PText);
        cv.setTextDatum(textdatum_t::top_center);
        cv.drawString(kTitles[s.placePage < 5 ? s.placePage : 0], kGridX + kGridW / 2, cellY(0) + 4);
        cv.setTextColor(PDim);
        cv.drawString("TAP=INFO  HOLD=PLACE  TAB=MORE", kGridX + kGridW / 2, cellY(0) + 18);
    }
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
}

}  // namespace rd
