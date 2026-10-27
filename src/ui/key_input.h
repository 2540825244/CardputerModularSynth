#pragma once
#include <cstdint>
#include "core/units.h"

enum class Side : uint8_t { Esc, Tab, Fn, Ctrl, Del, Backslash, Enter, Space };

struct KeyPos {
    int8_t x, y;  // physical: x 0..13 left to right, y 0..3 top to bottom
};

struct KeyEvent {
    enum Type : uint8_t { Down, Up, Hold } type;
    bool grid;
    uint8_t col, row;  // valid when grid
    Side side;         // valid when !grid
};

class KeyDecoder {
   public:
    // Diffs against the previous call; Hold fires once per press after kHoldMs.
    // Event order within a frame is key-index order (grid keys, then sides), so a same-frame
    // chord emits the grid Down before the modifier Down: read modifiers via sideHeld().
    void update(const KeyPos* pressed, int n, uint32_t nowMs, KeyEvent* out, int* outCount, int outMax);
    bool sideHeld(Side s) const { return held_[kGridKeys + static_cast<int>(s)]; }
    bool gridHeld(uint8_t col, uint8_t row) const {
        return col < kCols && row < kRows && held_[row * kCols + col];
    }

   private:
    static constexpr int kCols = 12, kRows = 4, kGridKeys = kCols * kRows, kKeys = kGridKeys + 8;
    bool held_[kKeys] = {};
    bool holdFired_[kKeys] = {};
    uint32_t downAt_[kKeys] = {};
};
