#pragma once
#include <cstddef>
#include <cstdint>
#include "core/engine.h"
#include "core/modules/keys.h"
#include "core/note_stack.h"
#include "core/patch.h"
#include "core/storage.h"
#include "core/undo.h"
#include "gesture.h"
#include "key_input.h"
#include "ui_state.h"

class UiController {
   public:
    UiController(Patch&, Engine&, UndoStack&, Storage&);
    void handle(const KeyEvent&, uint32_t nowMs);
    void tick(uint32_t nowMs);              // toast expiry, engine.collect (autosave: Task 17)
    // Hold-enter-and-tilt: while active, tilt() sets the param to start norm + degrees/60 (one
    // undo entry). Started by a Sweep on a selected param, ended by the Enter release.
    void tilt(float degreesSinceSweepStart);
    void beginTilt(uint16_t id, uint8_t paramIdx);
    void endTilt();
    bool tiltActive() const { return tilt_.active; }
    uint32_t tiltGeneration() const { return tiltGen_; }  // bumps on every beginTilt
    const UiState& state() const { return s_; }
    bool onPlayPage() const;  // the module view shows a Play page (grid keys play notes)

    // Shared edit helpers, used by every view file.
    // Structural edit: undo push, mutate, engine.apply, dirty. coalesceKey merges consecutive edits.
    bool edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey = 0);
    // Value-only edit (params, amounts): undo push (coalesced), mutate, dirty; no apply. The caller
    // pushes the value with engine.setParam / setOutAmount.
    bool editValue(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey);
    void toast(const char* msg, uint32_t nowMs);  // 1500 ms
    bool undo();                                  // pop, apply; false if nothing to undo
    bool redo();
    bool undoOrRedo();  // `\`: undo, or redo with fn held; toasts when there is nothing to do

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
    // Boot: the autosave, or else the BASIC preset (not dirty, no undo). Also reads the settings.
    bool bootLoad();
    // Load factory preset i (presets.h). keepUndo as in loadFile.
    bool loadPreset(int i, bool keepUndo, char* msg, size_t msgSize);
    bool loadJson(const std::string& text, bool keepUndo, char* msg, size_t msgSize);
    void newPatch();  // clears the patch (undoable)
    void loadSettings();
    void saveSettings();  // bumps settingsGen, writes /modsynth/settings.json
    uint32_t holdMs() const;
    void validate();  // end a carry / close a view whose module is gone (after undo, load, delete)
    int lastNote = 60;
    NoteStack notes;           // KEYS play page held notes
    bool auditioning = false;  // Space held in the patch view

   private:
    Patch& patch_;
    Engine& engine_;
    UndoStack& undo_;
    Storage& storage_;
    UiState s_;
    const KeyDecoder* dec_ = nullptr;
    GestureMapper mapper_;
    bool fn_ = false, ctrl_ = false;
    bool coalescedEdit_ = false;
    struct TiltState {
        bool active;
        uint16_t id;
        uint8_t idx;
        float startNorm;
        bool moved;  // left the +-1 degree deadband
    } tilt_ = {};
    uint32_t tiltGen_ = 0;
    uint32_t now_ = 0;
    bool autosaveBackoff_ = false, autosaveWarned_ = false;
    bool missingWarned_ = false, cpuFullWarned_ = false;
    uint32_t missingSinceMs_ = 0, cpuFullSinceMs_ = 0;
    uint32_t autosaveRetryMs_ = 0, autosaveWarnMs_ = 0;
};
