#pragma once
#include "controller.h"

// Per-view key handlers, called by UiController::handle.
void patchViewKey(UiController&, const KeyEvent&);
void placeViewKey(UiController&, const KeyEvent&);
void moduleViewKey(UiController&, const KeyEvent&);
void keysPlayKey(UiController&, const KeyEvent&);
void keysSettingsKey(UiController&, const KeyEvent&);
void fileViewKey(UiController&, const KeyEvent&);

inline int cellOf(const KeyEvent& e) { return e.row * kGridCols + e.col; }
