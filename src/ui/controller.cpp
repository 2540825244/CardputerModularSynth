#include "controller.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include "core/patch_io.h"
#include "views.h"

UiController::UiController(Patch& p, Engine& e, UndoStack& u, Storage& st)
    : patch_(p), engine_(e), undo_(u), storage_(st), s_{} {
    s_.view = View::Patch;
    s_.selCell = -1;
    s_.modCell = -1;
    s_.moveFromId = -1;
    s_.placeType = -1;
    s_.fileSlot = -1;
}

const ViewDef& viewDef(View v) {
    static const ViewDef kPatch{patchViewAction, false, true}, kModule{moduleViewAction, false, false},
        kPlace{placeViewAction, false, false}, kFile{fileViewAction, false, false},
        kKeysPlay{keysPlayAction, true, false}, kKeysSettings{keysSettingsAction, false, false};
    switch (v) {
        case View::Module: return kModule;
        case View::Place: return kPlace;
        case View::File: return kFile;
        case View::KeysPlay: return kKeysPlay;
        case View::KeysSettings: return kKeysSettings;
        case View::Patch: break;
    }
    return kPatch;
}

void UiController::handle(const KeyEvent& e, uint32_t nowMs) {
    now_ = nowMs;
    if (!e.grid && e.side == Side::Fn) fn_ = e.type != KeyEvent::Up;
    if (!e.grid && e.side == Side::Ctrl) ctrl_ = e.type != KeyEvent::Up;
    coalescedEdit_ = false;
    if (e.type == KeyEvent::Down) {
        // A second consecutive Esc cancels a carry and does nothing else; Del cancels it too.
        if (!e.grid && e.side == Side::Esc) {
            if (s_.carry.active && s_.escPresses >= 1) {
                s_.carry.active = false;
                s_.escPresses = 0;
                undo_.breakCoalescing();
                return;
            }
            if (s_.escPresses < 255) s_.escPresses++;
        } else {
            s_.escPresses = 0;
            if (!e.grid && e.side == Side::Del && s_.carry.active) {
                s_.carry.active = false;
                undo_.breakCoalescing();
                return;
            }
        }
    }
    if (auditioning && !e.grid && e.side == Side::Space && e.type == KeyEvent::Up) releaseAudition();
    // Gestures become intents in one place (gesture.h); the view current at each action answers
    // it. If an action changes the view, the rest of this event is dropped and keys still held
    // stay with the old view.
    const View before = s_.view;
    Action acts[2];
    const ViewDef& vd = viewDef(before);
    const int n = mapper_.map(e, fnHeld(), ctrlHeld() && vd.ctrlBlocksHold, vd.rawKeys, acts);
    for (int i = 0; i < n && s_.view == before; i++)
        if (vd.onAction(*this, acts[i])) {
            mapper_.consume(e);
            break;
        }
    validate();
    if (s_.view != before) mapper_.reset();
    if (tilt_.active && s_.view != View::Module && s_.view != View::KeysSettings) tilt_.active = false;
    if (auditioning && s_.view != View::Patch) releaseAudition();
    // Anything but a coalescing param edit (cursor move, view change) ends the run. A key release
    // changes nothing, so repeated taps of one fine key still merge; tilt drags (Task 19) end
    // their run explicitly on release.
    if (!coalescedEdit_ && e.type != KeyEvent::Up) undo_.breakCoalescing();
}

KeysModule* UiController::keysInstance() {
    for (int i = 0; i < patch_.moduleCount(); i++)
        if (patch_.moduleAt(i).type == &kKeysType)
            return static_cast<KeysModule*>(engine_.instance(patch_.moduleAt(i).id));
    return nullptr;
}

void UiController::releaseAudition() {
    auditioning = false;
    if (KeysModule* k = keysInstance()) k->gateOff();
}

void UiController::validate() {
    if (s_.carry.active && !patch_.find(s_.carry.id)) s_.carry.active = false;
    const bool inModule = s_.view == View::Module || s_.view == View::KeysPlay || s_.view == View::KeysSettings;
    if (inModule && !patch_.find(s_.openId)) {
        s_.view = View::Patch;
        notes.clear();
        s_.keysHeld = 0;
    }
    if (s_.moveFromId >= 0 && !patch_.find((uint16_t)s_.moveFromId)) s_.moveFromId = -1;
}

void UiController::tick(uint32_t nowMs) {
    if (s_.toast[0] && (int32_t)(nowMs - s_.toastUntilMs) >= 0) s_.toast[0] = 0;
    now_ = nowMs;
    engine_.collect();
    if (tilt_.active) return;  // no autosave mid-gesture
    if (!s_.dirty || (int32_t)(nowMs - s_.lastEditMs) < 2000) return;
    if (autosaveBackoff_ && (int32_t)(nowMs - autosaveRetryMs_) < 0) return;
    if (storage_.available() && storage_.write(slotPath(0).c_str(), patchToJson(patch_))) {
        s_.dirty = false;
        autosaveBackoff_ = autosaveWarned_ = false;
        return;
    }
    if (!autosaveWarned_) toast(storage_.available() ? "AUTOSAVE FAILED" : "NO SD", nowMs);
    autosaveWarned_ = true;
    autosaveBackoff_ = true;
    autosaveRetryMs_ = nowMs + 30000;
}

bool UiController::loadFile(const char* path, bool keepUndo, char* msg, size_t msgSize) {
    std::string text;
    if (!storage_.available()) {
        snprintf(msg, msgSize, "NO SD");
        return false;
    }
    if (!storage_.read(path, text)) {
        snprintf(msg, msgSize, "NO FILE");
        return false;
    }
    std::unique_ptr<Patch> loaded(new Patch());  // too big for the stack
    const LoadReport r = patchFromJson(text, *loaded, &engine_);
    if (r.error != LoadError::None) {
        snprintf(msg, msgSize, r.error == LoadError::NewerFormat ? "NEWER FW" : "BAD FILE");
        return false;
    }
    if (keepUndo) undo_.push(patch_, 0);
    patch_ = *loaded;
    engine_.apply(patch_);
    // Ids may now name different modules: drop every id-bound or position-bound UI reference.
    s_.carry.active = false;
    s_.view = View::Patch;
    s_.openId = 0;
    s_.moveFromId = -1;
    s_.selCell = -1;
    s_.modCell = -1;
    notes.clear();
    s_.keysHeld = 0;
    if (keepUndo) {
        s_.dirty = true;
        s_.lastEditMs = now_;
    } else {
        s_.dirty = false;
    }
    validate();
    if (r.skippedModules > 0) snprintf(msg, msgSize, "SKIPPED %d", r.skippedModules);
    else snprintf(msg, msgSize, "LOADED");
    return true;
}

bool UiController::loadAutosave() {
    const std::string path = slotPath(0);
    if (!storage_.exists(path.c_str())) return false;
    char msg[24];
    return loadFile(path.c_str(), false, msg, sizeof msg);
}

namespace {
struct TiltEdit {
    uint16_t id;
    uint8_t idx;
    float value;
};
void doTilt(Patch& p, void* c) {
    const TiltEdit* t = static_cast<TiltEdit*>(c);
    if (ModuleRec* m = p.find(t->id)) m->params[t->idx] = t->value;
}
}  // namespace

void UiController::beginTilt(uint16_t id, uint8_t idx) {
    const ModuleRec* m = patch_.find(id);
    if (!m || idx >= m->type->numParams) return;
    tilt_ = {true, id, idx, paramToNorm(m->type->params[idx], m->params[idx]), false};
    tiltGen_++;
}

void UiController::endTilt() {
    if (!tilt_.active) return;
    tilt_.active = false;
    if (s_.dirty) s_.lastEditMs = now_;  // autosave debounce counts from the release
    undo_.breakCoalescing();
}

void UiController::tilt(float degrees) {
    if (!tilt_.active) return;
    const ModuleRec* m = patch_.find(tilt_.id);
    if (!m) {
        tilt_.active = false;
        return;
    }
    if (!tilt_.moved && fabsf(degrees) <= 1.f) return;
    tilt_.moved = true;
    const ParamSpec& sp = m->type->params[tilt_.idx];
    const float v = paramFromNorm(sp, tilt_.startNorm + degrees / 60.f);
    if (v == m->params[tilt_.idx]) return;
    TiltEdit t{tilt_.id, tilt_.idx, v};
    editValue(doTilt, &t, paramKey(tilt_.id, tilt_.idx, false));
    engine_.setParam(tilt_.id, tilt_.idx, v);
}

bool UiController::edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey) {
    undo_.push(patch_, coalesceKey);
    fn(patch_, ctx);
    engine_.apply(patch_);
    s_.dirty = true;
    s_.lastEditMs = now_;
    if (coalesceKey) coalescedEdit_ = true;
    return true;
}

bool UiController::editValue(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey) {
    undo_.push(patch_, coalesceKey);
    fn(patch_, ctx);
    s_.dirty = true;
    s_.lastEditMs = now_;
    coalescedEdit_ = true;
    return true;
}

bool UiController::undo() {
    if (!undo_.undo(patch_)) return false;
    engine_.apply(patch_);
    s_.dirty = true;
    s_.lastEditMs = now_;
    return true;
}

void UiController::toast(const char* msg, uint32_t nowMs) {
    strncpy(s_.toast, msg, sizeof(s_.toast) - 1);
    s_.toast[sizeof(s_.toast) - 1] = 0;
    s_.toastUntilMs = nowMs + 1500;
}
