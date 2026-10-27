#include "views.h"

namespace {

constexpr uint8_t kOctParam = 0;  // KEYS param order: oct, glide, vel, legato

struct OctEdit {
    uint16_t id;
    float value;
};

void doSetOct(Patch& p, void* c) {
    const OctEdit* o = static_cast<OctEdit*>(c);
    if (ModuleRec* m = p.find(o->id)) m->params[kOctParam] = o->value;
}

void shiftOct(UiController& ui, const ModuleRec& m, int dir) {
    const float v = paramClamp(m.type->params[kOctParam], m.params[kOctParam] + (float)dir);
    OctEdit o{m.id, v};
    ui.editValue(doSetOct, &o, paramKey(m.id, kOctParam, false));
    ui.engine().setParam(m.id, kOctParam, v);
}

void allNotesOff(UiController& ui) {
    ui.notes.clear();
    if (KeysModule* k = static_cast<KeysModule*>(ui.engine().instance(ui.mut().openId))) k->gateOff();
}

}  // namespace

void keysPlayKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    const ModuleRec* m = ui.patch().find(s.openId);
    KeysModule* k = m ? static_cast<KeysModule*>(ui.engine().instance(m->id)) : nullptr;
    if (!m || !k) {
        s.view = View::Patch;
        return;
    }
    if (e.grid) {
        if (e.row == 0) {
            if (e.type != KeyEvent::Down) return;
            if (e.col == 0) shiftOct(ui, *m, -1);
            else if (e.col == 1) shiftOct(ui, *m, +1);
            else if (e.col == 11) {
                allNotesOff(ui);
                s.view = View::KeysSettings;
            }
            return;
        }
        if (e.type == KeyEvent::Hold) return;
        const int note = 48 + 12 * (3 - e.row) + e.col;
        if (e.type == KeyEvent::Down) {
            ui.notes.press(note);
            ui.lastNote = note;
            k->noteOn(note);
        } else {
            ui.notes.release(note);
            const int cur = ui.notes.current();
            if (cur >= 0) k->noteOn(cur);
            else k->gateOff();
        }
        return;
    }
    if (e.type != KeyEvent::Down) return;
    if (e.side == Side::Esc) {
        allNotesOff(ui);
        s.view = View::Patch;
    } else if (e.side == Side::Backslash) {
        ui.undo();
    }
}

void keysSettingsKey(UiController& ui, const KeyEvent& e) {
    if (!e.grid && e.type == KeyEvent::Down && e.side == Side::Enter) {
        ui.mut().view = View::KeysPlay;
        return;
    }
    moduleViewKey(ui, e);
}
