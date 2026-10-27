#pragma once
#include <cstddef>
#include <cstdint>
#include "core/engine.h"
#include "core/modules/keys.h"
#include "core/note_stack.h"
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
    // Structural edit: undo push, mutate, engine.apply, dirty. coalesceKey merges consecutive edits.
    bool edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey = 0);
    // Value-only edit (params, amounts): undo push (coalesced), mutate, dirty; no apply. The caller
    // pushes the value with engine.setParam / setOutAmount.
    bool editValue(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey);
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
    KeysModule* keysInstance();  // the KEYS instance, or nullptr
    void releaseAudition();
    // Load a patch file into the live patch. keepUndo pushes undo first and marks dirty; the boot
    // load does neither. Returns the toast text ("LOADED", "SKIPPED n", or the error) via msg.
    bool loadFile(const char* path, bool keepUndo, char* msg, size_t msgSize);
    bool loadAutosave();  // boot: no toast, not dirty
    void validate();  // end a carry / close a view whose module is gone (after undo, load, delete)
    int lastNote = 60;
    NoteStack notes;           // KeysPlay held notes
    bool auditioning = false;  // Space held in the patch view

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
    bool autosaveBackoff_ = false, autosaveWarned_ = false;
    uint32_t autosaveRetryMs_ = 0;
};
