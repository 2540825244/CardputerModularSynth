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
    // View changed: keys still held belong to the old view, so their hold/release do nothing;
    // a pending Enter is forgotten.
    void reset();
    void consume(const KeyEvent&);  // the rest of this key's gesture (hold, Enter release) does nothing

   private:
    static constexpr int kCells = 48;
    bool held_[kCells] = {};
    bool activated_[kCells] = {};  // this press already did its do-it (or was consumed)
    bool enterDown_ = false, enterSwept_ = false;
};
