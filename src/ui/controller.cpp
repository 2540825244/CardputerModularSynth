#include "controller.h"
#include <cstring>
#include "views.h"

UiController::UiController(Patch& p, Engine& e, UndoStack& u, Storage& st)
    : patch_(p), engine_(e), undo_(u), storage_(st), s_{} {
    s_.view = View::Patch;
    s_.selCell = -1;
    s_.modCell = -1;
    s_.moveFromId = -1;
    s_.placeType = -1;
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
    switch (s_.view) {
        case View::Patch: patchViewKey(*this, e); break;
        case View::Place: placeViewKey(*this, e); break;
        case View::Module: moduleViewKey(*this, e); break;
        default:  // module / keys / file views arrive in Tasks 15 and 17
            if (!e.grid && e.type == KeyEvent::Down && e.side == Side::Esc) s_.view = View::Patch;
            break;
    }
    validate();
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
    if (inModule && !patch_.find(s_.openId)) s_.view = View::Patch;
    if (s_.moveFromId >= 0 && !patch_.find((uint16_t)s_.moveFromId)) s_.moveFromId = -1;
}

void UiController::tick(uint32_t nowMs) {
    if (s_.toast[0] && (int32_t)(nowMs - s_.toastUntilMs) >= 0) s_.toast[0] = 0;
    engine_.collect();
}

void UiController::tilt(float) {}

bool UiController::edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey) {
    undo_.push(patch_, coalesceKey);
    fn(patch_, ctx);
    // A keyed edit changes values only: the caller pushes them with setParam/setOutAmount.
    if (!coalesceKey) engine_.apply(patch_);
    s_.dirty = true;
    s_.lastEditMs = now_;
    if (coalesceKey) coalescedEdit_ = true;
    return true;
}

bool UiController::undo() {
    if (!undo_.pop(patch_)) return false;
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
