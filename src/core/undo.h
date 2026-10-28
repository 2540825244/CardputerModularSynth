#pragma once
#include <cstdint>
#include "patch.h"
#include "units.h"

// Undo and redo share one ring: undo swaps the current patch into the slot it restores from, so
// redo needs no extra snapshots. A push after an undo drops the redo tail.
class UndoStack {
   public:
    void push(const Patch& before, uint32_t coalesceKey = 0);  // key != 0 and == last key: skipped
    bool undo(Patch& cur);                                     // false if nothing to undo
    bool redo(Patch& cur);                                     // false if nothing to redo
    void breakCoalescing() { lastKey_ = 0; }                   // any non-param action
    int size() const { return count_; }                        // <= kUndoDepth, oldest dropped
    int redoSize() const { return redo_; }

   private:
    void swapWith(Patch& cur, int slot);
    Patch ring_[kUndoDepth];
    int head_ = 0, count_ = 0, redo_ = 0;  // head_ = next write slot; redo entries follow it
    uint32_t lastKey_ = 0;
};

// kind: 0 param, 1 output amount, 2 input amount
static_assert(kMaxParams <= 32 && kMaxIns <= 32, "paramKey packs idx into 5 bits");
inline uint32_t paramKey(uint16_t id, uint8_t idx, uint8_t kind) {
    return (uint32_t(id) << 7 | uint32_t(idx) << 2 | uint32_t(kind)) + 1;
}
