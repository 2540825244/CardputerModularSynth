#pragma once
#include <cstdint>
#include "patch.h"
#include "units.h"

// Undo and redo share one byte arena of compact snapshots: only the modules in use, each with
// its own type's params and pins (~60 B for a small module, 148 B for GRD). Undo swaps the
// current patch into the entry it restores from, so redo needs no extra snapshots. A push after
// an undo drops the redo tail; when the arena is full the oldest steps go first.
class UndoStack {
   public:
    void push(const Patch& before, uint32_t coalesceKey = 0);  // key != 0 and == last key: skipped
    bool undo(Patch& cur);                                     // false if nothing to undo
    bool redo(Patch& cur);                                     // false if nothing to redo
    void breakCoalescing() { lastKey_ = 0; }                   // any non-param action
    int size() const { return u_; }                            // <= kUndoDepth, oldest dropped
    int redoSize() const { return n_ - u_; }

   private:
    static int encodedSize(const Patch&);
    static void encode(const Patch&, uint8_t* dst);
    static void decode(const uint8_t* src, Patch& out);
    int offsetOf(int idx) const;
    void erase(int idx);
    void insert(int idx, const Patch&);
    void makeRoom(int bytes, int* idx);  // drops the oldest undo steps, then the farthest redo
    void swapAt(Patch& cur, int idx);

    uint8_t arena_[kUndoBytes];
    uint16_t len_[kUndoDepth] = {};
    int n_ = 0, u_ = 0, used_ = 0;  // entries [0, u_) undo, [u_, n_) redo (u_ = next redo)
    uint32_t lastKey_ = 0;
};

// kind: 0 param, 1 output amount, 2 input amount
static_assert(kMaxParams <= 32 && kMaxIns <= 32, "paramKey packs idx into 5 bits");
inline uint32_t paramKey(uint16_t id, uint8_t idx, uint8_t kind) {
    return (uint32_t(id) << 7 | uint32_t(idx) << 2 | uint32_t(kind)) + 1;
}
