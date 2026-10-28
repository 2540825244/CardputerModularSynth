#include "core/registry.h"
#include "views.h"

namespace {

struct Add {
    const ModuleTypeInfo* type;
    uint8_t page, cell;
    PatchError err;
};

void doAdd(Patch& p, void* c) {
    Add* a = static_cast<Add*>(c);
    a->err = p.addModule(*a->type, a->page, a->cell, nullptr);
}

const char* errText(PatchError e) {
    switch (e) {
        case PatchError::CellTaken: return "CELL TAKEN";
        case PatchError::TooManyModules: return "TOO MANY";
        case PatchError::TooManyCables: return "TOO MANY";
        case PatchError::MaxInstances: return "MAX 1";
        default: return "CAN'T ADD";
    }
}

}  // namespace

namespace {

// The type shown at a grid cell of the place menu, -1 if none.
int typeAtCell(int cell) {
    int n = 0;
    allTypes(&n);
    const int row = cell / kGridCols, col = cell % kGridCols;
    return row == 1 && col < n ? col : -1;
}

void place(UiController& ui) {
    UiState& s = ui.mut();
    if (s.placeType < 0) return;
    const ModuleTypeInfo& t = *allTypes(nullptr)[s.placeType];
    const Refusal r = ui.engine().canAdd(ui.patch(), t);
    if (!r.ok) {
        ui.toast(r.reason, ui.now());
        return;
    }
    if (ui.patch().findAt(s.page, s.placeCell)) {
        ui.toast("CELL TAKEN", ui.now());
        return;
    }
    Add a{&t, s.page, s.placeCell, PatchError::None};
    ui.edit(doAdd, &a);
    if (a.err != PatchError::None) {
        ui.toast(errText(a.err), ui.now());
        return;
    }
    s.selCell = (int8_t)s.placeCell;
    s.placeType = -1;
    s.view = View::Patch;
}

}  // namespace

// Select a tile: show its details. Do-it on a tile (or enter on the shown one): place it.
bool placeViewAction(UiController& ui, const Action& a) {
    UiState& s = ui.mut();
    switch (a.act) {
        case Act::Select: {
            const int t = typeAtCell(a.cell);
            if (t >= 0) s.placeType = (int8_t)t;
            break;
        }
        case Act::Activate:
        case Act::Sweep:
            if (a.cell >= 0) {
                const int t = typeAtCell(a.cell);
                if (t < 0) break;
                s.placeType = (int8_t)t;
            }
            place(ui);
            break;
        case Act::Key:
            if (!a.key.grid && a.key.type == KeyEvent::Down && a.key.side == Side::Esc) {
                if (s.placeType >= 0) s.placeType = -1;
                else s.view = View::Patch;
            }
            break;
        case Act::SweepEnd: break;
    }
    return false;
}
