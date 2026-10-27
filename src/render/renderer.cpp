#include <esp_heap_caps.h>
#include <string.h>
#include "core/patch_io.h"
#include "render_internal.h"

namespace rd {

M5Canvas cv(&M5Cardputer.Display);

void drawIcon(Ic ic, int x, int y, uint8_t color) {
    const uint16_t* rows = kIcons[(int)ic];
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 12; c++)
            if (rows[r] & (1 << (11 - c))) cv.drawPixel(x + c, y + r, color);
}

void drawIcon(Icon ic, int x, int y, uint8_t color) {
    static const Ic map[] = {Ic::Keys, Ic::Osc, Ic::Vcf, Ic::Vca, Ic::Env, Ic::Lfo, Ic::Dly, Ic::Out};
    drawIcon(map[(int)ic], x, y, color);
}

void dotted(int x, int y, int w, int h, uint8_t color) {
    for (int i = 0; i < w; i += 2) {
        cv.drawPixel(x + i, y, color);
        cv.drawPixel(x + i, y + h - 1, color);
    }
    for (int j = 0; j < h; j += 2) {
        cv.drawPixel(x, y + j, color);
        cv.drawPixel(x + w - 1, y + j, color);
    }
}

void smallText(const char* s, int cx, int y, uint8_t color) {
    cv.setFont(&fonts::TomThumb);
    cv.setTextColor(color);
    cv.setTextDatum(textdatum_t::top_center);
    cv.drawString(s, cx, y);
}

const char* keyLabel(int row, int col) {
    static const char* const r0[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
    static const char* const r1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]"};
    static const char* const r2[] = {"AA", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"};
    static const char* const r3[] = {"OP", "AL", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"};
    static const char* const* const rows[] = {r0, r1, r2, r3};
    return rows[row & 3][col % kGridCols];
}

void emptyCell(int col, int row, const char* label, bool selected) {
    const int x = cellX(col), y = cellY(row), w = cellW(col), h = cellH(row);
    dotted(x, y, w, h, PBorder);
    if (label) smallText(label, x + w / 2, y + h / 2 - 2, PFaint);
    if (selected) outline(col, row, PAccent);
}

void outline(int col, int row, uint8_t color) {
    cv.drawRect(cellX(col), cellY(row), cellW(col), cellH(row), color);
}

void moduleTile(const ModuleTypeInfo& t, int col, int row, bool dim, int iconDy) {
    const int x = cellX(col), y = cellY(row), w = cellW(col);
    const uint8_t fg = dim ? PFaint : PText;
    cv.fillRect(x + 2, y + 2, w - 4, 2, dim ? PFaint : PText);  // category bar
    drawIcon(t.icon, x + (w - 12) / 2, y + 8 + iconDy, fg);
    smallText(t.name, x + w / 2, y + 22, fg);
}

void drawSides(const Side4& s) {
    cv.fillRect(0, 0, kStripW, kH, PStrip);
    cv.fillRect(kW - kStripW, 0, kStripW, kH, PStrip);
    for (int i = 0; i < 4; i++) {
        const int cy = cellY(i) + cellH(i) / 2;
        if (i == 2 && s.fnActive) cv.fillRect(0, cellY(i), kStripW, cellH(i), PAccent);
        if (i == 1 && s.tabText) smallText(s.tabText, 8, cy - 2, PText);
        else if (s.leftIcon[i]) drawIcon(s.left[i], 2, cy - 6, i == 2 && s.fnActive ? PBg : PText);
        if (s.rightIcon[i]) drawIcon(s.right[i], kW - kStripW + 2, cy - 6, PText);
    }
}

// Bottom-up fill, 2 px wide.
void drawBars(float cpu, float ram) {
    auto bar = [](int x, float f) {
        if (f < 0.f) f = 0.f;
        if (f > 1.f) f = 1.f;
        const int h = (int)(f * kH + 0.5f);
        if (h > 0) cv.fillRect(x, kH - h, 2, h, PAccent);
    };
    bar(0, cpu);
    bar(kW - 2, ram);
}

void drawToast(const char* msg) {
    if (!msg[0]) return;
    cv.fillRect(kGridX + 2, 2, kGridW - 4, 14, PAccent);
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PBg);
    cv.setTextDatum(textdatum_t::middle_center);
    cv.drawString(msg, kGridX + kGridW / 2, 9);
}

}  // namespace rd

static bool g_ready = false;
static uint32_t g_spriteBytes = 0;

bool renderBegin() {
    rd::cv.setColorDepth(4);
    if (!rd::cv.createSprite(rd::kW, rd::kH)) return false;
    for (int i = 0; i < 16; i++) {
        const uint32_t c = kStyleB[i];
        rd::cv.setPaletteColor(i, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
    }
    g_spriteBytes = (rd::kW * rd::kH + 1) / 2;
    g_ready = true;
    return true;
}

uint32_t renderSpriteBytes() { return g_spriteBytes; }

void renderFrame(const UiState& s, const Patch& p, Engine& e, Storage& st, uint32_t nowMs) {
    if (!g_ready) return;
    using namespace rd;
    // File-slot presence is cached: refreshed on entering the file view and when a toast appears.
    static bool filled[12];
    static View lastView = View::Patch;
    static uint32_t lastToast = 0;
    if (s.view == View::File && (lastView != View::File || (s.toast[0] && s.toastUntilMs != lastToast))) {
        for (int i = 0; i < 12; i++) filled[i] = st.exists(slotPath(i + 1).c_str());
    }
    lastView = s.view;
    lastToast = s.toastUntilMs;

    cv.fillSprite(PBg);
    switch (s.view) {
        case View::Patch: drawPatchView(s, p, e); break;
        case View::Place: drawPlaceView(s, p, e); break;
        case View::File: drawFileView(s, filled); break;
        default: drawModulePlaceholder(s, p); break;
    }
    const float total = (float)heap_caps_get_total_size(MALLOC_CAP_8BIT);
    drawBars(e.cpuLoad(), total > 0 ? 1.f - heap_caps_get_free_size(MALLOC_CAP_8BIT) / total : 0.f);
    if (s.toast[0] && (int32_t)(nowMs - s.toastUntilMs) < 0) drawToast(s.toast);
    cv.pushSprite(0, 0);
}
