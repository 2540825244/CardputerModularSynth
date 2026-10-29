#include <stdio.h>
#include <string.h>
#include "core/patch_io.h"
#include "core/presets.h"
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
void drawDetails(const ModuleTypeInfo& t, const Refusal& r, uint32_t cpuUsed, uint32_t cost) {
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
    joinLabels(out, sizeof out, t.outLabels, t.numOuts > 4 ? 4 : t.numOuts);
    snprintf(line, sizeof line, "IN %s%s  OUT %s%s", in, t.numIns > 4 ? ".." : "", out, t.numOuts > 4 ? ".." : "");
    cv.setTextColor(PText);
    cv.drawString(line, x + 3, y + 12);
    if (!r.ok) {
        cv.setTextColor(PAccent);
        cv.drawString(r.reason, x + 3, y + 22);
        return;
    }
    // CPU: what this type would take of the budget (measured once it has run), on top of what is used.
    const int bw = 60, bx = x + 3, by = y + 24;
    cv.drawRect(bx, by, bw, 5, PDim);
    const int used = (int)(bw * (float)cpuUsed / (float)kCpuBudget);
    const int add = (int)(bw * (float)cost / (float)kCpuBudget) + 1;
    cv.fillRect(bx, by, used > bw ? bw : used, 5, PDim);
    cv.fillRect(bx + used, by, used + add > bw ? bw - used : add, 5, PAccent);
    smallText("CPU", bx + bw + 9, by, PDim);
    cv.setTextDatum(textdatum_t::top_right);
    cv.setTextColor(PDim);
    cv.drawString("ENTER=PLACE", x + w - 3, y + 22);
}

}  // namespace

void drawPlaceView(const UiState& s, const Patch& p, Engine& e) {
    const Engine::Live live = e.live();  // once per frame, not per tile
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    for (int row = 1; row < kGridRows; row++)
        for (int col = 0; col < kGridCols; col++) {
            const int t = placeTypeAt(s.placePage, (uint8_t)row, (uint8_t)col);
            if (t < 0) {
                emptyCell(col, row, nullptr, false);
                continue;
            }
            moduleTile(*types[t], col, row, !e.canAddLive(p, *types[t], live).ok, 0);
            if (t == s.placeType) outline(col, row, PAccent);
        }
    if (s.placeType >= 0 && s.placeType < n) {
        const uint32_t est = e.estimatedCycles();
        drawDetails(*types[s.placeType], e.canAddLive(p, *types[s.placeType], live),
                    est > live.cycles ? est : live.cycles, e.costOf(*types[s.placeType]));
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

namespace {

void bandText(const char* big, const char* hint) {
    const int y = cellY(3), h = cellH(3);
    cv.fillRect(cellX(0), y, cellX(10) + cellW(10) - cellX(0), h, PStrip);
    cv.setFont(&fonts::Font0);
    cv.setTextDatum(textdatum_t::middle_left);
    cv.setTextColor(PText);
    cv.setTextSize(2);
    cv.drawString(big, cellX(0) + 4, y + h / 2);
    cv.setTextSize(1);
    cv.setTextColor(PDim);
    cv.drawString(hint, cellX(0) + 4 + cv.textWidth(big) * 2 + 8, y + h / 2);
}

void slotsPage(const UiState& s, const bool filled[kSlots]) {
    for (int cell = 0; cell < kSlots; cell++) {
        const int col = cell % kGridCols, row = cell / kGridCols;
        char label[4];
        snprintf(label, sizeof label, "%d", cell + 1);
        const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
        if (filled[cell]) {
            cv.fillRect(x, y, w, h, PEmpty);
            drawIcon(Ic::File, x + (w - 12) / 2, y + 6, PText);
            smallText(label, x + w / 2, y + 22, PText);
        } else {
            emptyCell(col, row, label, false);
        }
        if (cell == s.fileSlot) outline(col, row, PAccent);
    }
    const bool save = s.filePage == 1;
    char hint[32];
    if (s.fileSlot >= 0 && s.fileSlot < kSlots)
        snprintf(hint, sizeof hint, "SLOT %d %s", s.fileSlot + 1, filled[s.fileSlot] ? (save ? "OVERWRITE?" : "") : "EMPTY");
    else
        snprintf(hint, sizeof hint, "%s", save ? "PICK A SLOT, ENTER" : "PICK, ENTER  TAB=MORE");
    bandText(save ? "SAVE" : "LOAD", hint);
    if (!save) {  // NEW: an empty patch
        const int x = cellX(11), y = cellY(3), w = cellW(11), h = cellH(3);
        cv.fillRect(x, y, w, h, PEmpty);
        drawIcon(Ic::Plus, x + (w - 12) / 2, y + 6, PText);
        smallText("NEW", x + w / 2, y + 22, PText);
        if (s.fileSlot == 47) outline(11, 3, PAccent);
    }
}

void presetsPage(const UiState& s) {
    int n = 0;
    const Preset* ps = presets(&n);
    for (int cell = 0; cell < 36; cell++) {
        const int col = cell % kGridCols, row = cell / kGridCols;
        if (cell >= n) {
            emptyCell(col, row, nullptr, false);
            continue;
        }
        const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
        cv.fillRect(x, y, w, h, PEmpty);
        cv.fillRect(x + 2, y + 2, w - 4, 2, PAccent);
        char a[4] = {0}, b[4] = {0};
        strncpy(a, ps[cell].name, 3);
        if (strlen(ps[cell].name) > 3) strncpy(b, ps[cell].name + 3, 3);
        smallText(a, x + w / 2, y + 10, PText);
        smallText(b, x + w / 2, y + 18, PText);
        if (cell == s.fileSlot) outline(col, row, PAccent);
    }
    bandText(s.fileSlot >= 0 && s.fileSlot < n ? ps[s.fileSlot].name : "PRESETS",
             s.fileSlot >= 0 && s.fileSlot < n ? "ENTER = LOAD" : "FACTORY PATCHES");
}

void systemPage(const UiState& s) {
    const Settings& st = s.settings;
    auto tile = [](int col, const char* label, bool on) {
        const int x = cellX(col), y = cellY(0), w = cellW(col), h = cellH(0);
        cv.fillRect(x, y, w, h, on ? PAccent : PEmpty);
        smallText(label, x + w / 2, y + h / 2 - 2, on ? PBg : PText);
    };
    for (int col = 0; col < kGridCols; col++) emptyCell(col, 0, nullptr, false);
    tile(0, st.theme ? "A" : "B", true);
    for (int i = 1; i <= 5; i++) {
        char l[2] = {(char)('0' + i), 0};
        tile(1 + i, l, i <= st.brightness);
    }
    static const char* const kHold[] = {"300", "400", "600"};
    for (int i = 0; i < 3; i++) tile(8 + i, kHold[i], i == st.hold);
    smallText("THEME", cellX(0) + cellW(0) / 2, cellY(1) + 4, PDim);
    smallText("BRIGHTNESS", (cellX(2) + cellX(6) + cellW(6)) / 2, cellY(1) + 4, PDim);
    smallText("HOLD MS", (cellX(8) + cellX(10) + cellW(10)) / 2, cellY(1) + 4, PDim);
    bandText("SYSTEM", "TAP TO SET");
}

}  // namespace

void drawFileView(const UiState& s, const bool filled[kSlots]) {
    switch (s.filePage) {
        case 0:
        case 1: slotsPage(s, filled); break;
        case 2: presetsPage(s); break;
        default: systemPage(s); break;
    }
}

}  // namespace rd
