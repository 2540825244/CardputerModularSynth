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
    ui.mut().keysHeld = 0;
    if (KeysModule* k = static_cast<KeysModule*>(ui.engine().instance(ui.mut().openId))) k->gateOff();
}

}  // namespace

namespace {

void playKey(UiController& ui, const KeyEvent& e) {
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
        const uint64_t bit = 1ull << ((3 - e.row) * 12 + e.col);
        if (e.type == KeyEvent::Down) {
            s.keysHeld |= bit;
            ui.notes.press(note);
            ui.lastNote = note;
            k->noteOn(note);
        } else {
            s.keysHeld &= ~bit;
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

}  // namespace

// The play view is the exception to the do-it rule: every grid key plays a note (raw keys).
bool keysPlayAction(UiController& ui, const Action& a) {
    playKey(ui, a.key);
    return false;
}

// Settings page: the module view, with esc flipping back to the play page.
bool keysSettingsAction(UiController& ui, const Action& a) {
    if (a.act == Act::Key && !a.key.grid && a.key.type == KeyEvent::Down && a.key.side == Side::Esc) {
        ui.mut().view = View::KeysPlay;
        ui.mut().escPresses = 0;  // a page flip, not a step back: the next esc leaves (carry kept)
        return false;
    }
    return moduleViewAction(ui, a);
}
