#pragma once
#include <M5Cardputer.h>
#include "core/patch.h"
#include "icons.h"
#include "palette.h"
#include "renderer.h"

namespace rd {

constexpr int kW = 240, kH = 135;
constexpr int kStripW = 16, kGridX = 16, kGridW = 208;

extern M5Canvas cv;

inline int cellX(int col) { return kGridX + col * kGridW / kGridCols; }
inline int cellY(int row) { return row * kH / kGridRows; }
inline int cellW(int col) { return cellX(col + 1) - cellX(col) - 1; }
inline int cellH(int row) { return cellY(row + 1) - cellY(row) - 1; }

void drawIcon(Ic ic, int x, int y, uint8_t color);
void drawIcon(Icon ic, int x, int y, uint8_t color);  // module icon
void dotted(int x, int y, int w, int h, uint8_t color);
void smallText(const char* s, int cx, int y, uint8_t color);  // TomThumb, centred
void emptyCell(int col, int row, const char* label, bool selected);
// Module tile: category bar, icon, name. dy shifts the icon (LFO animation).
void moduleTile(const ModuleTypeInfo&, int col, int row, bool dim, int iconDy);
void outline(int col, int row, uint8_t color);

// Key-cap labels per grid cell (row 0..3, col 0..11), nullptr-free.
const char* keyLabel(int row, int col);

struct Side4 {
    bool leftIcon[4], rightIcon[4];
    Ic left[4], right[4];
    bool fnActive;
    const char* tabText;  // drawn in the tab slot instead of an icon when non-null
};
void drawSides(const Side4&);
void drawBars(float cpu, float ram);
void drawToast(const char* msg);

void drawPatchView(const UiState&, const Patch&, Engine&);
void drawPlaceView(const UiState&, const Patch&, Engine&);
void drawFileView(const UiState&, const bool filled[12]);
void drawModulePlaceholder(const UiState&, const Patch&);

}  // namespace rd
