#include "core/patch_io.h"
#include "views.h"

void fileViewKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    if (e.type != KeyEvent::Down) return;
    if (e.grid) {
        if (e.row != 0) return;
        const int slot = e.col + 1;
        const std::string path = slotPath(slot);
        if (s.fileSaveMode) {
            if (!ui.storage().available()) ui.toast("NO SD", ui.now());
            else if (ui.storage().write(path.c_str(), patchToJson(ui.patch()))) ui.toast("SAVED", ui.now());
            else ui.toast("SAVE FAILED", ui.now());
        } else {
            char msg[24];
            ui.loadFile(path.c_str(), true, msg, sizeof msg);
            ui.toast(msg, ui.now());
        }
        return;
    }
    if (e.side == Side::Tab) s.fileSaveMode = !s.fileSaveMode;
    else if (e.side == Side::Esc) s.view = View::Patch;
}
