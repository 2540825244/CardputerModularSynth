#pragma once
#include <cstdint>
#include "key_input.h"

// The one place gestures become intents. fn+tap, holding a grid key, and tap-then-enter all mean
// "do it" (Activate); a plain tap only selects. Holding enter is a sweep (hold-and-tilt). Views
// answer Actions, so remapping a gesture never touches a view.
enum class Act : uint8_t {
    Key,       // raw passthrough: side keys other than Enter, grid Up, and everything in raw mode
    Select,    // grid Down: put the cursor on `cell`
    Activate,  // "do it": cell >= 0 a grid cell, cell == -1 the current selection (from Enter)
    Sweep,     // Enter held >= kHoldMs
    SweepEnd,  // Enter released after a Sweep
};

struct Action {
    Act act;
    int8_t cell;   // row * 12 + col for grid events (Select, Activate, grid Up), -1 otherwise
    KeyEvent key;  // the raw event that produced this action
};

class GestureMapper {
   public:
    // raw = true (Keys play view): every event becomes one Act::Key and pending Enter/activation
    // state is forgotten. Returns the number of actions written to out (<= 2).
    int map(const KeyEvent&, bool fnHeld, bool ctrlHeld, bool raw, Action* out);
    void reset();  // forget pending Enter and per-key activation state (view changes)

   private:
    static constexpr int kCells = 48;
    bool activated_[kCells] = {};
    bool enterDown_ = false, enterSwept_ = false;
};
