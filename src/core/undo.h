#pragma once
#include <cstdint>
#include "patch.h"
#include "units.h"

class UndoStack {
   public:
    void push(const Patch& before, uint32_t coalesceKey = 0);  // key != 0 and == last key: skipped
    bool pop(Patch& into);                                     // false if empty
    void breakCoalescing() { lastKey_ = 0; }                   // any non-param action
    int size() const { return count_; }                        // <= kUndoDepth, oldest dropped

   private:
    Patch ring_[kUndoDepth];
    int head_ = 0, count_ = 0;  // head_ = next write slot
    uint32_t lastKey_ = 0;
};

inline uint32_t paramKey(uint16_t id, uint8_t idx, bool amount) {
    return (uint32_t(id) << 9 | uint32_t(idx) << 1 | uint32_t(amount)) + 1;
}
