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
struct Pair {
    uint16_t a, b;
};
void doSwap(Patch& p, void* c) {
    const Pair* s = static_cast<Pair*>(c);
    p.swapModules(s->a, s->b);
}
struct Copy {
    uint16_t id;
    uint8_t page, cell;
    PatchError err;
};
void doCopy(Patch& p, void* c) {
    Copy* k = static_cast<Copy*>(c);
    k->err = p.copyModule(k->id, k->page, k->cell, nullptr);
}

// Moves module `id` to `cell` on the current page; onto another module it swaps the two.
void moveTo(UiController& ui, uint16_t id, int cell) {
    UiState& s = ui.mut();
    const ModuleRec* at = ui.patch().findAt(s.page, (uint8_t)cell);
    if (at && at->id == id) return;
    if (at) {
        Pair pr{id, at->id};
        ui.edit(doSwap, &pr);
    } else {
        Cell c{id, s.page, (uint8_t)cell};
        ui.edit(doMove, &c);
    }
    s.selCell = (int8_t)cell;
}

// Places a copy of module `id` at `cell` (params and amounts, no cables).
void copyTo(UiController& ui, uint16_t id, int cell) {
    UiState& s = ui.mut();
    const ModuleRec* src = ui.patch().find(id);
    if (!src) return;
    if (ui.patch().findAt(s.page, (uint8_t)cell)) {
        ui.toast("CELL TAKEN", ui.now());
        return;
    }
    const Refusal r = ui.engine().canAdd(ui.patch(), *src->type);
    if (!r.ok) {
        ui.toast(r.reason, ui.now());
        return;
    }
    Copy k{id, s.page, (uint8_t)cell, PatchError::None};
    ui.edit(doCopy, &k);
    if (k.err != PatchError::None) ui.toast(k.err == PatchError::MaxInstances ? "MAX 1" : "CAN'T COPY", ui.now());
    else s.selCell = (int8_t)cell;
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
        const uint16_t id = (uint16_t)s.moveFromId;
        s.moveFromId = -1;
        if (ui.fnHeld()) copyTo(ui, id, cell);  // fn+target: a copy instead of a move
        else moveTo(ui, id, cell);
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
    s.modPage = 0;  // KEYS opens on its PLAY page
    s.view = View::Module;
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
            ui.undoOrRedo();
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

// ctrl tapped alone: start moving the selected module, or cancel a pending move.
void ctrlTap(UiController& ui) {
    UiState& s = ui.mut();
    if (s.moveFromId >= 0) {
        s.moveFromId = -1;
        return;
    }
    const ModuleRec* m = s.selCell >= 0 ? ui.patch().findAt(s.page, (uint8_t)s.selCell) : nullptr;
    if (!m) return;
    s.moveFromId = m->id;
    ui.toast("MOVE: PICK A CELL", ui.now());
}

}  // namespace

bool patchViewAction(UiController& ui, const Action& a) {
    switch (a.act) {
        case Act::Select: return select(ui, a.cell);
        case Act::Chord: {
            // Hold a module, press another cell: move it there (or swap with a module there).
            const ModuleRec* m = ui.patch().findAt(ui.state().page, (uint8_t)a.from);
            if (m && ui.state().moveFromId < 0) moveTo(ui, m->id, a.cell);
            return true;
        }
        case Act::SideTap:
            if (a.key.side == Side::Ctrl) ctrlTap(ui);
            break;
        case Act::Activate:
        case Act::Sweep: activate(ui, a.cell >= 0 ? a.cell : ui.state().selCell); break;
        case Act::Key:
            if (!a.key.grid) sideKey(ui, a.key);
            break;
        case Act::SweepEnd: break;
    }
    return false;
}
