#include "core/patch_io.h"
#include "core/presets.h"
#include "views.h"

namespace {

enum { kLoad, kSave, kPresets, kPresets2, kSystem, kFilePages };  // v1.4: 36 presets a page
constexpr int kPresetsPerPage = 36;
constexpr int kNewCell = 47;  // LOAD page, bottom-right: a new empty patch

// Do-it on the selected cell of the LOAD / SAVE / PRESETS (1 and 2) page.
void run(UiController& ui) {
    UiState& s = ui.mut();
    if (s.fileSlot < 0) return;
    char msg[24];
    if (s.filePage == kPresets || s.filePage == kPresets2) {
        if (s.fileSlot >= kPresetsPerPage) return;  // the bottom row is the band, not presets
        const int i = s.fileSlot + (s.filePage - kPresets) * kPresetsPerPage;
        if (ui.loadPreset(i, true, msg, sizeof msg)) ui.toast(msg, ui.now());
        else ui.toast(msg, ui.now());
        return;
    }
    if (s.filePage == kLoad && s.fileSlot == kNewCell) {
        ui.newPatch();
        return;
    }
    if (s.fileSlot >= kSlots) return;
    const std::string path = slotPath(s.fileSlot + 1);
    if (s.filePage == kSave) {
        if (!ui.storage().available()) {
            ui.toast("NO SD", ui.now());
            return;
        }
        const std::string json = patchToJson(ui.patch());  // once: ~16 KB on a tight heap
        if (json.empty()) ui.toast("NO RAM", ui.now());
        else if (!patchFits(json)) ui.toast("PATCH TOO BIG", ui.now());
        else if (ui.storage().write(path.c_str(), json)) ui.toast("SAVED", ui.now());
        else ui.toast("SAVE FAILED", ui.now());
    } else {
        ui.loadFile(path.c_str(), true, msg, sizeof msg);
        ui.toast(msg, ui.now());
    }
}

// SYSTEM page, row 0: col 0 theme, cols 2..6 brightness 1..5, cols 8..10 hold 300/400/600 ms;
// row 2: cols 0..4 screen sleep OFF/1/2/5/10 min. A tap applies at once: settings are harmless
// to change.
void system(UiController& ui, int cell) {
    Settings& st = ui.mut().settings;
    if (cell == 0) st.theme ^= 1;
    else if (cell >= 2 && cell <= 6) st.brightness = (uint8_t)(cell - 1);
    else if (cell >= 8 && cell <= 10) st.hold = (uint8_t)(cell - 8);
    else if (cell >= 24 && cell <= 28) st.sleep = (uint8_t)(cell - 24);
    else return;
    ui.saveSettings();
}

}  // namespace

// Tap a cell to select it; do-it loads, saves or loads the preset, so a stray tap never
// overwrites. tab / fn+tab flips LOAD, SAVE, PRESETS, PRESETS 2, SYSTEM.
bool fileViewAction(UiController& ui, const Action& a) {
    UiState& s = ui.mut();
    switch (a.act) {
        case Act::Select:
            if (s.filePage == kSystem) system(ui, a.cell);
            else if ((s.filePage == kPresets || s.filePage == kPresets2) && a.cell >= kPresetsPerPage) s.fileSlot = -1;
            else s.fileSlot = a.cell;
            break;
        case Act::Activate:
        case Act::Sweep:
            if (s.filePage == kSystem) break;
            if (a.cell >= 0) s.fileSlot = a.cell;
            run(ui);
            break;
        case Act::Key: {
            const KeyEvent& e = a.key;
            if (e.grid || e.type != KeyEvent::Down) break;
            if (e.side == Side::Tab) {
                s.filePage = (uint8_t)((s.filePage + kFilePages + (ui.fnHeld() ? -1 : 1)) % kFilePages);
                s.fileSlot = -1;  // a slot picked to load is never saved over by one enter
            } else if (e.side == Side::Esc) {
                s.view = View::Patch;
            } else if (e.side == Side::Backslash) {
                ui.undoOrRedo();
            }
            break;
        }
        case Act::SweepEnd:
        case Act::Chord:
        case Act::SideTap: break;
    }
    return false;
}
