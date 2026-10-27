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

void placeViewKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    if (e.type != KeyEvent::Down) return;  // Hold/Up carry no meaning here
    int n = 0;
    const ModuleTypeInfo* const* types = allTypes(&n);
    if (e.grid) {
        if (e.row == 1 && e.col < n) s.placeType = (int8_t)e.col;
        return;
    }
    if (e.side == Side::Esc) {
        if (s.placeType >= 0) s.placeType = -1;
        else s.view = View::Patch;
        return;
    }
    if (e.side != Side::Enter || s.placeType < 0) return;
    const ModuleTypeInfo& t = *types[s.placeType];
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
