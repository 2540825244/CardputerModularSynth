#include "undo.h"

namespace {
Patch gSwap;  // too big for the UI task stack
}

void UndoStack::push(const Patch& before, uint32_t coalesceKey) {
    if (coalesceKey != 0 && coalesceKey == lastKey_) return;
    lastKey_ = coalesceKey;
    ring_[head_] = before;
    head_ = (head_ + 1) % kUndoDepth;
    if (count_ < kUndoDepth) count_++;
    redo_ = 0;
}

void UndoStack::swapWith(Patch& cur, int slot) {
    gSwap = ring_[slot];
    ring_[slot] = cur;
    cur = gSwap;
}

bool UndoStack::undo(Patch& cur) {
    if (count_ == 0) return false;
    head_ = (head_ + kUndoDepth - 1) % kUndoDepth;
    swapWith(cur, head_);
    count_--;
    redo_++;
    lastKey_ = 0;
    return true;
}

bool UndoStack::redo(Patch& cur) {
    if (redo_ == 0) return false;
    swapWith(cur, head_);
    head_ = (head_ + 1) % kUndoDepth;
    count_++;
    redo_--;
    lastKey_ = 0;
    return true;
}
