#pragma once
#include <cstdint>
#include "core/engine.h"
#include "core/patch.h"
#include "core/storage.h"
#include "core/undo.h"
#include "key_input.h"
#include "ui_state.h"

class UiController {
   public:
    UiController(Patch&, Engine&, UndoStack&, Storage&);
    void handle(const KeyEvent&, uint32_t nowMs);
    void tick(uint32_t nowMs);              // toast expiry, engine.collect (autosave: Task 17)
    void tilt(float degreesSinceHoldStart); // Task 19
    const UiState& state() const { return s_; }

    // Shared edit helpers, used by every view file.
    // Undo push, mutate, engine.apply, dirty. A non-zero coalesceKey marks a value-only edit: it
    // merges consecutive edits and skips apply (the caller calls engine.setParam/setOutAmount).
    bool edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey = 0);
    void toast(const char* msg, uint32_t nowMs);  // 1500 ms
    bool undo();                                  // pop, apply; false if nothing to undo

    // Used by the view files.
    UiState& mut() { return s_; }
    Patch& patch() { return patch_; }
    Engine& engine() { return engine_; }
    UndoStack& undoStack() { return undo_; }
    Storage& storage() { return storage_; }
    // Modifier state comes from the decoder (state after update()), because a same-frame chord
    // emits the grid Down before the modifier Down. Without a decoder, falls back to event tracking.
    void setDecoder(const KeyDecoder* d) { dec_ = d; }
    bool fnHeld() const { return dec_ ? dec_->sideHeld(Side::Fn) : fn_; }
    bool ctrlHeld() const { return dec_ ? dec_->sideHeld(Side::Ctrl) : ctrl_; }
    uint32_t now() const { return now_; }
    int lastNote = 60;

   private:
    Patch& patch_;
    Engine& engine_;
    UndoStack& undo_;
    Storage& storage_;
    UiState s_;
    const KeyDecoder* dec_ = nullptr;
    bool fn_ = false, ctrl_ = false;
    bool coalescedEdit_ = false;
    uint32_t now_ = 0;
};
