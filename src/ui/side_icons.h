#pragma once
#include <cstdint>
#include "core/patch.h"
#include "render/icons.h"
#include "ui_state.h"

// Every side-strip icon, decided in one place from the UI state. Index = (int)Side:
// left strip esc, tab, fn, ctrl; right strip del, \, enter, space.
struct SideIcons {
    Ic icon[8];
    bool shown[8];
    bool active[8];      // drawn lit (accent), e.g. fn while a cable is carried
    const char* name[8];  // <= 6 letters, for the help overlay
    char tabText[8];      // drawn in the tab slot instead of an icon when non-empty ("2/3")
};
SideIcons sideIcons(const UiState&, const Patch&, bool fnHeld);

// fn held alone for >= 600 ms (no other key pressed since): show the side-key help overlay.
bool helpVisible(const UiState&, uint32_t nowMs);
// A side slot draws pressed while its key is held and for 120 ms after a Down.
bool sidePressed(const UiState&, int side, uint32_t nowMs);
