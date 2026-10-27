#pragma once
#include "controller.h"

// Per-view key handlers, called by UiController::handle.
void patchViewKey(UiController&, const KeyEvent&);
void placeViewKey(UiController&, const KeyEvent&);
void moduleViewKey(UiController&, const KeyEvent&);

inline int cellOf(const KeyEvent& e) { return e.row * kGridCols + e.col; }
