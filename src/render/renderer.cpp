#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include "core/patch_io.h"
#include "ui/anim.h"
#include "ui/layout.h"
#include "render_internal.h"

namespace rd {

M5Canvas cv(&M5Cardputer.Display);
uint32_t gNow = 0;
int gTheme = 0;

uint8_t categoryColor(Category c) {
    if (!gTheme) return PText;
    switch (c) {
        case Category::Source: return PAccent;
        case Category::Modulator: return PSpare0;
        default: return PSpare1;
    }
}

void drawIcon(Ic ic, int x, int y, uint8_t color) {
    const uint16_t* rows = kIcons[(int)ic];
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 12; c++)
            if (rows[r] & (1 << (11 - c))) cv.drawPixel(x + c, y + r, color);
}

void drawIcon(Icon ic, int x, int y, uint8_t color) {
    Ic i = Ic::Plus;  // a module icon without art yet
    switch (ic) {
        case Icon::Keys: i = Ic::Keys; break;
        case Icon::Osc: i = Ic::Osc; break;
        case Icon::Vcf: i = Ic::Vcf; break;
        case Icon::Vca: i = Ic::Vca; break;
        case Icon::Env: i = Ic::Env; break;
        case Icon::Lfo: i = Ic::Lfo; break;
        case Icon::Dly: i = Ic::Dly; break;
        case Icon::Out: i = Ic::Out; break;
        case Icon::Mix: i = Ic::Mix; break;
        case Icon::Atn: i = Ic::Atn; break;
        case Icon::Snh: i = Ic::Snh; break;
        case Icon::Clk: i = Ic::Clk; break;
        case Icon::Mux: i = Ic::Mux; break;
        case Icon::Dmx: i = Ic::Dmx; break;
        case Icon::Tilt: i = Ic::Tilt; break;
        default:  // v1.2 icons: Icon::Rev.. and Ic::MRev.. are in the same order
            if ((int)ic >= (int)Icon::Rev) i = (Ic)((int)Ic::MRev + ((int)ic - (int)Icon::Rev));
            break;
    }
    drawIcon(i, x, y, color);
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
    if (gTheme) cv.fillRect(x, y, w, cellH(row), PEmpty);
    cv.fillRect(x + 2, y + 2, w - 4, 2, dim ? PFaint : categoryColor(t.category));  // category bar
    drawIcon(t.icon, x + (w - 12) / 2, y + 8 + iconDy, fg);
    smallText(t.name, x + w / 2, y + 22, fg);
}

void drawSides(const UiState& st, const Patch& p, bool fnHeld) {
    const SideIcons si = sideIcons(st, p, fnHeld);
    cv.fillRect(0, 0, kStripW, kH, PStrip);
    cv.fillRect(kW - kStripW, 0, kStripW, kH, PStrip);
    for (int i = 0; i < 8; i++) {
        const int row = i % 4;
        const int x = i < 4 ? 0 : kW - kStripW;
        const int y = cellY(row), h = cellH(row), cy = y + h / 2;
        const bool pressed = sidePressed(st, i, gNow);
        const bool lit = si.active[i] && (!pressed || (gNow / 250) % 2 == 0);
        uint8_t fg = si.shown[i] ? PText : PFaint;
        if (pressed) {
            cv.fillRoundRect(x + 1, y + 1, kStripW - 2, h - 2, 3, PText);
            fg = PBg;
        } else if (lit) {
            cv.fillRect(x, y, kStripW, h, PAccent);
            fg = PBg;
        }
        if (i == (int)Side::Tab && si.tabText[0]) smallText(si.tabText, x + 8, cy - 2, fg);
        else if (si.shown[i]) drawIcon(si.icon[i], x + 2, cy - 6, fg);
    }
}

void drawHelp(const UiState& st, const Patch& p, bool fnHeld) {
    const SideIcons si = sideIcons(st, p, fnHeld);
    static const char* const kKeys[8] = {"ESC", "TAB", "FN", "CTRL", "DEL", "\\", "ENTER", "SPACE"};
    cv.fillRoundRect(kGridX + 4, 4, kGridW - 8, kH - 8, 4, PBg);
    cv.drawRoundRect(kGridX + 4, 4, kGridW - 8, kH - 8, 4, PText);
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PText);
    for (int i = 0; i < 8; i++) {
        const int row = i % 4;
        const int cy = cellY(row) + cellH(row) / 2;
        const char* name = si.shown[i] && si.name[i] ? si.name[i] : "-";
        if (i < 4) {
            cv.setTextDatum(textdatum_t::middle_left);
            cv.setTextColor(PDim);
            cv.drawString(kKeys[i], kGridX + 10, cy);
            cv.setTextColor(PText);
            cv.drawString(name, kGridX + 46, cy);
        } else {
            cv.setTextDatum(textdatum_t::middle_right);
            cv.setTextColor(PDim);
            cv.drawString(kKeys[i], kGridX + kGridW - 10, cy);
            cv.setTextColor(PText);
            cv.drawString(name, kGridX + kGridW - 46, cy);
        }
    }
    smallText("HOLD FN = HELP   FN+KEY = ALT", kGridX + kGridW / 2, kH - 14, PDim);
}

void drawCarryStrip(const UiState& st, const Patch& p) {
    const ModuleRec* src = p.find(st.carry.id);
    char line[40] = "HOLDING CABLE";
    if (src) {
        const char* pin = st.carry.fromOutput ? (st.carry.pin < src->type->numOuts ? src->type->outLabels[st.carry.pin] : "?")
                                              : (st.carry.pin < src->type->numIns ? src->type->inLabels[st.carry.pin] : "?");
        snprintf(line, sizeof line, "HOLDING %s.%s  FN DROPS  ESC CANCELS", src->type->name, pin);
    }
    cv.fillRect(kGridX, 0, kGridW, 8, PAccent);
    const uint8_t col = PCable0 + st.carry.color % kNumCableColors;
    if ((gNow / 250) % 2 == 0) cv.fillCircle(kGridX + 5, 4, 2, col);
    smallText(line, kGridX + kGridW / 2 + 4, 1, PBg);
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

// Slides down from above the screen over the first 100 ms.
void drawToast(const char* msg, uint32_t shownAt) {
    if (!msg[0]) return;
    const float t = 1.f - flashT(shownAt, gNow, 100);
    const int y = -14 + (int)(16.f * t);
    cv.fillRoundRect(kGridX + 2, y, kGridW - 4, 14, 3, PAccent);
    cv.setFont(&fonts::Font0);
    cv.setTextColor(PBg);
    cv.setTextDatum(textdatum_t::middle_center);
    cv.drawString(msg, kGridX + kGridW / 2, y + 7);
}

// Zoom outline between a tile and the whole grid when a module view opens or closes.
void drawZoom(const UiState& s, const Patch& p) {
    const float z = zoomT(s.viewMs, gNow);
    if (z >= 1.f || s.viewMs == 0) return;
    const bool opening = s.view == View::Module && s.prevView == View::Patch;
    const bool closing = s.view == View::Patch && s.prevView == View::Module;
    const bool placing = s.view == View::Place && s.prevView == View::Patch;
    if (!opening && !closing && !placing) return;
    int cell = s.selCell;
    if (opening || closing)
        if (const ModuleRec* m = p.find(s.openId))
            if (m->page == s.page) cell = m->cell;
    if (placing) cell = s.placeCell;
    if (cell < 0) return;
    const int col = cell % kGridCols, row = cell / kGridCols;
    const float a = closing ? 1.f - z : z;  // 0 = tile, 1 = grid
    const int x0 = cellX(col), y0 = cellY(row), w0 = cellW(col), h0 = cellH(row);
    const int x = x0 + (int)((kGridX - x0) * a), y = y0 + (int)((0 - y0) * a);
    const int w = w0 + (int)((kGridW - w0) * a), h = h0 + (int)((kH - h0) * a);
    cv.drawRect(x, y, w, h, PAccent);
    cv.drawRect(x + 1, y + 1, w - 2, h - 2, PAccent);
}

// A grid key flashes its cell for 90 ms after its Down.
void drawPressFlash(const UiState& s, bool playPage) {
    if (s.pressCell < 0 || playPage) return;
    const float f = flashT(s.pressMs, gNow, 90);
    if (f <= 0.f) return;
    const int col = s.pressCell % kGridCols, row = s.pressCell / kGridCols;
    const int inset = (int)((1.f - f) * 4.f);
    cv.drawRect(cellX(col) + inset, cellY(row) + inset, cellW(col) - 2 * inset, cellH(row) - 2 * inset, PAccent);
}

}  // namespace rd

static bool g_ready = false;
static uint32_t g_spriteBytes = 0;

void renderSetTheme(int theme) {
    rd::gTheme = theme ? 1 : 0;
    const uint32_t* pal = rd::gTheme ? kStyleA : kStyleB;
    for (int i = 0; i < 16; i++) {
        const uint32_t c = pal[i];
        rd::cv.setPaletteColor(i, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
    }
}

void renderSetBrightness(int level) {
    level = level < 1 ? 1 : (level > 5 ? 5 : level);
    static const uint8_t kLevels[] = {30, 70, 120, 180, 255};
    M5Cardputer.Display.setBrightness(kLevels[level - 1]);
}

bool renderBegin() {
    rd::cv.setColorDepth(4);
    if (!rd::cv.createSprite(rd::kW, rd::kH)) return false;
    renderSetTheme(0);
    g_spriteBytes = (rd::kW * rd::kH + 1) / 2;
    g_ready = true;
    return true;
}

uint32_t renderSpriteBytes() { return g_spriteBytes; }

void renderFrame(const UiState& s, const Patch& p, Engine& e, Storage& st, uint32_t nowMs, bool fnHeld) {
    if (!g_ready) return;
    using namespace rd;
    // File-slot presence is cached: all slots on entering the file view (36 SD lookups), then
    // only the slot a SAVE just wrote (nothing else adds or removes a file).
    static bool filled[kSlots];
    static View lastView = View::Patch;
    static uint32_t lastToast = 0;
    if (s.view == View::File && lastView != View::File) {
        for (int i = 0; i < kSlots; i++) filled[i] = st.exists(slotPath(i + 1).c_str());
    } else if (s.view == View::File && s.toast[0] && s.toastUntilMs != lastToast && !strcmp(s.toast, "SAVED") &&
               s.fileSlot >= 0 && s.fileSlot < kSlots) {
        filled[s.fileSlot] = true;
    }
    lastView = s.view;
    lastToast = s.toastUntilMs;

    gNow = nowMs;
    cv.fillSprite(PBg);
    switch (s.view) {
        case View::Patch: drawPatchView(s, p, e); break;
        case View::Place: drawPlaceView(s, p, e); break;
        case View::File: drawFileView(s, filled); break;
        case View::Module: drawModuleView(s, p, e); break;
    }
    drawPressFlash(s, s.view == View::Module && p.find(s.openId) &&
                          modulePageKind(*p.find(s.openId)->type, s.modPage) == PageKind::Play);
    drawZoom(s, p);
    drawSides(s, p, fnHeld);
    if (s.view == View::Patch && s.carry.active) drawCarryStrip(s, p);
    if (helpVisible(s, nowMs)) drawHelp(s, p, fnHeld);
    const float total = (float)heap_caps_get_total_size(MALLOC_CAP_8BIT);
    drawBars(e.cpuLoad(), total > 0 ? 1.f - heap_caps_get_free_size(MALLOC_CAP_8BIT) / total : 0.f);
    if (s.toast[0] && (int32_t)(nowMs - s.toastUntilMs) < 0) drawToast(s.toast, s.toastUntilMs - 1500);
    cv.pushSprite(0, 0);
}
