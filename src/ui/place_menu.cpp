#include "core/registry.h"
#include "layout.h"
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

int typeAtCell(const UiState& s, int cell) {
    return placeTypeAt(s.placePage, (uint8_t)(cell / kGridCols), (uint8_t)(cell % kGridCols));
}

// force (fn+enter): past NO CPU / NO RAM. A forced module that has no heap yet waits in the
// patch, silent ("NO RAM"), and starts once there is room; pool slots and MAX 1 still hold.
void place(UiController& ui, bool force) {
    UiState& s = ui.mut();
    if (s.placeType < 0) return;
    const ModuleTypeInfo& t = *allTypes(nullptr)[s.placeType];
    const Refusal r = ui.engine().canAddLive(ui.patch(), t);
    const Refusal hard = ui.engine().canLoad(ui.patch(), t);
    if (!r.ok && (!force || !hard.ok)) {
        ui.toast(force ? hard.reason : r.reason, ui.now());
        return;
    }
    if (ui.patch().findAt(s.page, s.placeCell)) {
        ui.toast("CELL TAKEN", ui.now());
        return;
    }
    if (ui.patch().moduleCount() >= kMaxModules) {
        ui.toast("TOO MANY", ui.now());
        return;
    }
    Add a{&t, s.page, s.placeCell, PatchError::None};
    ui.edit(doAdd, &a);
    if (a.err != PatchError::None) {
        ui.toast(errText(a.err), ui.now());
        return;
    }
    if (!r.ok) ui.toast("FORCED", ui.now());
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
            const int t = typeAtCell(s, a.cell);
            if (t >= 0) s.placeType = (int8_t)t;
            break;
        }
        case Act::Activate:
        case Act::Sweep:
            if (a.cell >= 0) {
                const int t = typeAtCell(s, a.cell);
                if (t < 0) break;
                s.placeType = (int8_t)t;
            }
            place(ui, a.cell < 0 && ui.fnHeld());  // fn+enter forces; fn+tile is the plain do-it
            break;
        case Act::Key:
            if (a.key.grid || a.key.type != KeyEvent::Down) break;
            if (a.key.side == Side::Esc) {
                if (s.placeType >= 0) s.placeType = -1;
                else s.view = View::Patch;
            } else if (a.key.side == Side::Tab) {  // next / previous category page
                const int n = placePageCount();
                s.placePage = (uint8_t)((s.placePage + n + (ui.fnHeld() ? -1 : 1)) % n);
                s.placeType = -1;
            }
            break;
        case Act::SweepEnd:
        case Act::Chord:
        case Act::SideTap: break;
    }
    return false;
}
