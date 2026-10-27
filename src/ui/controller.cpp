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
    switch (s_.view) {
        case View::Patch: patchViewKey(*this, e); break;
        case View::Place: placeViewKey(*this, e); break;
        default:  // module / keys / file views arrive in Tasks 15 and 17
            if (!e.grid && e.type == KeyEvent::Down && e.side == Side::Esc) s_.view = View::Patch;
            break;
    }
    // Anything but a coalescing param edit (cursor move, key release, view change) ends the run.
    if (!coalescedEdit_) undo_.breakCoalescing();
}

void UiController::tick(uint32_t nowMs) {
    if (s_.toast[0] && (int32_t)(nowMs - s_.toastUntilMs) >= 0) s_.toast[0] = 0;
    engine_.collect();
}

void UiController::tilt(float) {}

bool UiController::edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey) {
    undo_.push(patch_, coalesceKey);
    fn(patch_, ctx);
    engine_.apply(patch_);
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
