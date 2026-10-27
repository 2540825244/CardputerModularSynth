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

void gridDown(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    const int cell = cellOf(e);
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
        return;
    }
    s.selCell = (int8_t)cell;
    if (m && ui.ctrlHeld()) {
        s.moveFromId = m->id;
    } else if (!m && ui.fnHeld()) {
        openPlace(ui, cell);
    }
}

}  // namespace

void patchViewKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    if (e.grid) {
        if (e.type == KeyEvent::Down) {
            gridDown(ui, e);
        } else if (e.type == KeyEvent::Hold) {
            const int cell = cellOf(e);
            if (s.moveFromId < 0 && !ui.ctrlHeld() && !ui.patch().findAt(s.page, (uint8_t)cell))
                openPlace(ui, cell);
        }
        return;
    }
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
        case Side::Enter: {
            const ModuleRec* m = s.selCell >= 0 ? ui.patch().findAt(s.page, (uint8_t)s.selCell) : nullptr;
            if (!m) break;
            s.openId = m->id;
            s.modCell = -1;
            s.view = m->type == &kKeysType ? View::KeysPlay : View::Module;
            break;
        }
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
            }
            break;
        default: break;
    }
}
