#include "core/modules/keys.h"
#include "views.h"

namespace {

struct Cell {
    uint16_t id;
    uint8_t page, cell;
};

void doRemove(Patch& p, void* c) { p.removeModule(static_cast<Cell*>(c)->id); }
void doMove(Patch& p, void* c) {
    const Cell* m = static_cast<Cell*>(c);
    p.moveModule(m->id, m->page, m->cell);
}

void openPlace(UiController& ui, int cell) {
    UiState& s = ui.mut();
    s.view = View::Place;
    s.placeCell = (uint8_t)cell;
    s.placeType = -1;
    s.selCell = (int8_t)cell;
}

// Select: put the cursor on the cell, finish a pending move there, or start one with ctrl.
// Returns true when it finished a move (the press is used up).
bool select(UiController& ui, int cell) {
    UiState& s = ui.mut();
    const ModuleRec* m = ui.patch().findAt(s.page, (uint8_t)cell);
    if (s.moveFromId >= 0) {
        if (m) {
            ui.toast("CELL TAKEN", ui.now());
        } else {
            Cell c{(uint16_t)s.moveFromId, s.page, (uint8_t)cell};
            ui.edit(doMove, &c);
            s.moveFromId = -1;
            s.selCell = (int8_t)cell;
        }
        return true;
    }
    s.selCell = (int8_t)cell;
    if (m && ui.ctrlHeld()) s.moveFromId = m->id;
    return false;
}

// Do-it: an empty cell opens the place menu, a module opens its view.
void activate(UiController& ui, int cell) {
    UiState& s = ui.mut();
    if (cell < 0 || s.moveFromId >= 0 || ui.ctrlHeld()) return;
    s.selCell = (int8_t)cell;
    const ModuleRec* m = ui.patch().findAt(s.page, (uint8_t)cell);
    if (!m) {
        openPlace(ui, cell);
        return;
    }
    s.openId = m->id;
    s.modCell = -1;
    s.view = m->type == &kKeysType ? View::KeysPlay : View::Module;
}

void sideKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    const bool down = e.type == KeyEvent::Down;
    if (e.side == Side::Space) {
        // The release is handled by UiController::handle, whatever view is active by then.
        if (down)
            if (KeysModule* k = ui.keysInstance()) {
                k->noteOn(ui.lastNote);
                ui.auditioning = true;
            }
        return;
    }
    if (!down) return;
    switch (e.side) {
        case Side::Tab:
            s.page = (uint8_t)((s.page + (ui.fnHeld() ? kPages - 1 : 1)) % kPages);
            s.selCell = -1;  // a pending move survives, so a module can change page
            break;
        case Side::Del: {
            const ModuleRec* m = s.selCell >= 0 ? ui.patch().findAt(s.page, (uint8_t)s.selCell) : nullptr;
            if (!m) break;
            Cell c{m->id, 0, 0};
            s.moveFromId = -1;
            ui.edit(doRemove, &c);
            break;
        }
        case Side::Backslash:
            s.moveFromId = -1;
            if (!ui.undo()) ui.toast("NOTHING TO UNDO", ui.now());
            break;
        case Side::Esc:
            if (s.moveFromId >= 0) {
                s.moveFromId = -1;
            } else {
                s.view = View::File;
                s.fileSaveMode = false;
                s.fileSlot = -1;
            }
            break;
        default: break;
    }
}

}  // namespace

bool patchViewAction(UiController& ui, const Action& a) {
    switch (a.act) {
        case Act::Select: return select(ui, a.cell);
        case Act::Activate:
        case Act::Sweep: activate(ui, a.cell >= 0 ? a.cell : ui.state().selCell); break;
        case Act::Key:
            if (!a.key.grid) sideKey(ui, a.key);
            break;
        case Act::SweepEnd: break;
    }
    return false;
}
