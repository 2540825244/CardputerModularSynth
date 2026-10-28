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
    Chord,     // grid Down while another grid key is held (not yet activated): `from` -> `cell`
    SideTap,   // fn or ctrl pressed and released alone (no grid key, side key or hold in between)
};

struct Action {
    Act act;
    int8_t cell;   // row * 12 + col for grid events (Select, Activate, Chord, grid Up), -1 otherwise
    KeyEvent key;  // the raw event that produced this action
    int8_t from;   // Chord: the held cell, -1 otherwise
};

class GestureMapper {
   public:
    // holdBlocked: a grid hold does not do-it (the patch view's ctrl+module move).
    // raw = true (Keys play page): every event becomes one Act::Key and pending Enter/activation
    // state is forgotten. Returns the number of actions written to out (<= 2).
    // A grid Down while another grid key is held and not yet activated emits Select + Chord (no
    // Activate, even with fn), and neither key's hold fires afterwards.
    int map(const KeyEvent&, bool fnHeld, bool holdBlocked, bool raw, Action* out);
    // View changed: keys still held belong to the old view, so their hold/release do nothing;
    // a pending Enter is forgotten.
    void reset();
    void consume(const KeyEvent&);  // the rest of this key's gesture (hold, Enter release) does nothing

   private:
    static constexpr int kCells = 48;
    bool held_[kCells] = {};
    bool activated_[kCells] = {};  // this press already did its do-it (or was consumed)
    bool enterDown_ = false, enterSwept_ = false;
    // fn / ctrl: down, and whether the press was used (a grid key, another side key, or its hold)
    bool modDown_[2] = {}, modUsed_[2] = {};
    int heldFree(int except) const;  // a held, not activated grid cell other than `except`, or -1
};
