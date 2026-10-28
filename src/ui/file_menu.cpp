#include "core/patch_io.h"
#include "views.h"

namespace {

// Loads or saves the selected slot (column 0..11 -> slot 1..12).
void run(UiController& ui) {
    UiState& s = ui.mut();
    if (s.fileSlot < 0) return;
    const std::string path = slotPath(s.fileSlot + 1);
    if (s.fileSaveMode) {
        if (!ui.storage().available()) ui.toast("NO SD", ui.now());
        else if (ui.storage().write(path.c_str(), patchToJson(ui.patch()))) ui.toast("SAVED", ui.now());
        else ui.toast("SAVE FAILED", ui.now());
    } else {
        char msg[24];
        ui.loadFile(path.c_str(), true, msg, sizeof msg);
        ui.toast(msg, ui.now());
    }
}

}  // namespace

// Tap a slot (top row) to select it; do-it loads or saves it, so a stray tap never overwrites.
bool fileViewAction(UiController& ui, const Action& a) {
    UiState& s = ui.mut();
    switch (a.act) {
        case Act::Select:
            if (a.cell < kGridCols) s.fileSlot = a.cell;
            break;
        case Act::Activate:
        case Act::Sweep:
            if (a.cell >= kGridCols) break;
            if (a.cell >= 0) s.fileSlot = a.cell;
            run(ui);
            break;
        case Act::Key: {
            const KeyEvent& e = a.key;
            if (e.grid || e.type != KeyEvent::Down) break;
            if (e.side == Side::Tab) {
                s.fileSaveMode = !s.fileSaveMode;
                s.fileSlot = -1;  // a slot picked to load is never saved over by one enter
            }
            else if (e.side == Side::Esc) s.view = View::Patch;
            else if (e.side == Side::Backslash && !ui.undo()) ui.toast("NOTHING TO UNDO", ui.now());
            break;
        }
        case Act::SweepEnd: break;
    }
    return false;
}
