#include "undo.h"

void UndoStack::push(const Patch& before, uint32_t coalesceKey) {
    if (coalesceKey != 0 && coalesceKey == lastKey_) return;
    lastKey_ = coalesceKey;
    ring_[head_] = before;
    head_ = (head_ + 1) % kUndoDepth;
    if (count_ < kUndoDepth) count_++;
}

bool UndoStack::pop(Patch& into) {
    if (count_ == 0) return false;
    head_ = (head_ + kUndoDepth - 1) % kUndoDepth;
    into = ring_[head_];
    count_--;
    lastKey_ = 0;
    return true;
}
