#pragma once
#include <M5Cardputer.h>
#include "core/patch.h"
#include "icons.h"
#include "palette.h"
#include "renderer.h"
#include "ui/key_input.h"
#include "ui/side_icons.h"

namespace rd {

constexpr int kW = 240, kH = 135;
constexpr int kStripW = 16, kGridX = 16, kGridW = 208;

extern M5Canvas cv;
extern uint32_t gNow;  // frame time, for pulsing
extern int gTheme;     // 0 style B, 1 style A
uint8_t categoryColor(Category);

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

// Both side strips from sideIcons(); held keys draw inverted (src/ui/side_icons.h).
void drawSides(const UiState&, const Patch&, bool fnHeld);
void drawHelp(const UiState&, const Patch&, bool fnHeld);  // fn-held overlay naming each side key
void drawCarryStrip(const UiState&, const Patch&);          // patch view: thin "holding" banner
void drawBars(float cpu, float ram);
void drawToast(const char* msg, uint32_t shownAt);

void drawPatchView(const UiState&, const Patch&, Engine&);
void drawPlaceView(const UiState&, const Patch&, Engine&);
void drawFileView(const UiState&, const bool filled[36]);
void drawModuleView(const UiState&, const Patch&, Engine&);  // Module and KeysSettings
void drawKeysPlay(const UiState&, const Patch&);

}  // namespace rd
