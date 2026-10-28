#include "undo.h"
#include <cstddef>
#include <cstring>

namespace {
Patch gSwap;  // too big for the UI task stack

constexpr size_t kHead = offsetof(ModuleRec, params);  // type, id, page, cell
constexpr int kPatchHead = 4;                          // nextId, module count, cable count

int counts(const ModuleTypeInfo& t, int* np, int* no, int* ni) {
    *np = t.numParams < kMaxParams ? t.numParams : kMaxParams;
    *no = t.numOuts < kMaxOuts ? t.numOuts : kMaxOuts;
    *ni = t.numIns < kMaxIns ? t.numIns : kMaxIns;
    return (int)kHead + (*np + *no + *ni) * (int)sizeof(float);
}
}  // namespace

static_assert(kMaxModules < 256 && kMaxCables < 256, "counts packed in a byte");

int UndoStack::encodedSize(const Patch& p) {
    int n = kPatchHead + p.nCables_ * (int)sizeof(CableRec);
    int np, no, ni;
    for (int i = 0; i < p.nModules_; i++) n += counts(*p.modules_[i].type, &np, &no, &ni);
    return n;
}

void UndoStack::encode(const Patch& p, uint8_t* d) {
    memcpy(d, &p.nextId, 2);
    d[2] = (uint8_t)p.nModules_;
    d[3] = (uint8_t)p.nCables_;
    d += kPatchHead;
    for (int i = 0; i < p.nModules_; i++) {
        const ModuleRec& r = p.modules_[i];
        int np, no, ni;
        counts(*r.type, &np, &no, &ni);
        memcpy(d, &r, kHead);
        d += kHead;
        memcpy(d, r.params, np * sizeof(float));
        d += np * sizeof(float);
        memcpy(d, r.outAmount, no * sizeof(float));
        d += no * sizeof(float);
        memcpy(d, r.inAmount, ni * sizeof(float));
        d += ni * sizeof(float);
    }
    memcpy(d, p.cables_, p.nCables_ * sizeof(CableRec));
}

void UndoStack::decode(const uint8_t* s, Patch& p) {
    p.clear();
    memcpy(&p.nextId, s, 2);
    p.nModules_ = s[2];
    p.nCables_ = s[3];
    s += kPatchHead;
    for (int i = 0; i < p.nModules_; i++) {
        ModuleRec& r = p.modules_[i];
        memcpy(&r, s, kHead);
        s += kHead;
        int np, no, ni;
        counts(*r.type, &np, &no, &ni);
        memcpy(r.params, s, np * sizeof(float));
        s += np * sizeof(float);
        for (int k = 0; k < kMaxOuts; k++) r.outAmount[k] = 1.f;  // unused pins read 1, as addModule leaves them
        for (int k = 0; k < kMaxIns; k++) r.inAmount[k] = 1.f;
        memcpy(r.outAmount, s, no * sizeof(float));
        s += no * sizeof(float);
        memcpy(r.inAmount, s, ni * sizeof(float));
        s += ni * sizeof(float);
    }
    memcpy(p.cables_, s, p.nCables_ * sizeof(CableRec));
}

int UndoStack::offsetOf(int idx) const {
    int o = 0;
    for (int i = 0; i < idx; i++) o += len_[i];
    return o;
}

void UndoStack::erase(int idx) {
    const int o = offsetOf(idx), l = len_[idx];
    memmove(arena_ + o, arena_ + o + l, used_ - o - l);
    used_ -= l;
    for (int i = idx; i + 1 < n_; i++) len_[i] = len_[i + 1];
    n_--;
}

// Caller made room: n_ < kUndoDepth and used_ + encodedSize(p) <= kUndoBytes.
void UndoStack::insert(int idx, const Patch& p) {
    const int o = offsetOf(idx), l = encodedSize(p);
    memmove(arena_ + o + l, arena_ + o, used_ - o);
    encode(p, arena_ + o);
    used_ += l;
    for (int i = n_; i > idx; i--) len_[i] = len_[i - 1];
    len_[idx] = (uint16_t)l;
    n_++;
}

void UndoStack::makeRoom(int bytes, int* idx) {
    while (n_ > 0 && (n_ >= kUndoDepth || used_ + bytes > kUndoBytes)) {
        if (*idx > 0) {  // the oldest undo step
            erase(0);
            (*idx)--;
            u_--;
        } else {  // the farthest redo step
            erase(n_ - 1);
        }
    }
}

void UndoStack::push(const Patch& before, uint32_t coalesceKey) {
    if (coalesceKey != 0 && coalesceKey == lastKey_) return;
    lastKey_ = coalesceKey;
    while (n_ > u_) erase(n_ - 1);  // a new edit drops the redo tail
    int idx = u_;
    makeRoom(encodedSize(before), &idx);
    insert(idx, before);
    u_ = idx + 1;
}

// Entry idx becomes cur, and cur takes its place (for undo the first redo step, for redo the
// newest undo step).
void UndoStack::swapAt(Patch& cur, int idx) {
    decode(arena_ + offsetOf(idx), gSwap);
    erase(idx);
    const bool wasUndo = idx < u_;
    if (wasUndo) u_--;  // entries before idx stay undo steps
    makeRoom(encodedSize(cur), &idx);
    insert(idx, cur);
    if (!wasUndo) u_ = idx + 1;
    cur = gSwap;
}

bool UndoStack::undo(Patch& cur) {
    if (u_ == 0) return false;
    swapAt(cur, u_ - 1);
    lastKey_ = 0;
    return true;
}

bool UndoStack::redo(Patch& cur) {
    if (u_ == n_) return false;
    swapAt(cur, u_);
    lastKey_ = 0;
    return true;
}
