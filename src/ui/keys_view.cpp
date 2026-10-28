#include "core/scales.h"
#include "keys_layout.h"
#include "views.h"

namespace {

enum { kOctParam = 0, kLayoutParam = 4, kScaleParam = 5, kRootParam = 6, kLatchParam = 7 };  // KEYS params

struct ParamEdit {
    uint16_t id;
    uint8_t idx;
    float value;
};

void doSet(Patch& p, void* c) {
    const ParamEdit* o = static_cast<ParamEdit*>(c);
    if (ModuleRec* m = p.find(o->id)) m->params[o->idx] = o->value;
}

void setParam(UiController& ui, const ModuleRec& m, uint8_t idx, float v) {
    v = paramClamp(m.type->params[idx], v);
    ParamEdit o{m.id, idx, v};
    ui.editValue(doSet, &o, paramKey(m.id, idx, 0));
    ui.engine().setParam(m.id, idx, v);
}

}  // namespace

void keysAllNotesOff(UiController& ui) {
    ui.notes.clear();
    ui.mut().keysHeld = 0;
    ui.mut().latchedBit = -1;
    if (KeysModule* k = static_cast<KeysModule*>(ui.engine().instance(ui.mut().openId))) k->gateOff();
}

void keysLeavePage(UiController& ui) {
    // A latched note is a drone to tweak other modules over: it keeps sounding. Only the same
    // key, DEL (panic), a layout change or LATCH OFF release it.
    if (ui.state().latchedBit < 0) keysAllNotesOff(ui);
}

int keysNoteFor(const ModuleRec& m, int row, int col) {
    return keysNoteAt((int)m.params[kLayoutParam], (int)m.params[kScaleParam], (int)m.params[kRootParam], row, col);
}

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
        if (e.type == KeyEvent::Hold) return;
        const int note = keysNoteFor(*m, e.row, e.col);
        if (note < 0) return;
        const int bit = e.row * kGridCols + e.col;
        const bool latch = m->params[kLatchParam] >= 0.5f;
        if (latch) {  // a tap holds its note until another key (moves it) or the same key (stops)
            if (e.type != KeyEvent::Down) return;
            if (s.latchedBit == bit) {
                keysAllNotesOff(ui);
                return;
            }
            s.latchedBit = (int8_t)bit;
            s.keysHeld = 1ull << bit;
            ui.lastNote = note;
            k->noteOn(note);
            return;
        }
        if (e.type == KeyEvent::Down) {
            s.keysHeld |= 1ull << bit;
            ui.notes.press(note);
            ui.lastNote = note;
            k->noteOn(note);
        } else {
            if (!(s.keysHeld & (1ull << bit))) return;  // pressed before this page opened
            s.keysHeld &= ~(1ull << bit);
            ui.notes.release(note);
            const int cur = ui.notes.current();
            if (cur >= 0) k->noteOn(cur);
            else k->gateOff();
        }
        return;
    }
    if (e.type != KeyEvent::Down) return;
    switch (e.side) {
        case Side::Esc:
            keysLeavePage(ui);
            s.view = View::Patch;
            break;
        case Side::Ctrl:
        case Side::Space:
            setParam(ui, *m, kOctParam, m->params[kOctParam] + (e.side == Side::Space ? 1.f : -1.f));
            break;
        case Side::Enter: {  // next layout; fn+enter the previous one. Held notes are released.
            const int n = kLayCount, cur = (int)m->params[kLayoutParam];
            const int next = (cur + n + (ui.fnHeld() ? -1 : 1)) % n;
            keysAllNotesOff(ui);
            setParam(ui, *m, kLayoutParam, (float)next);
            ui.toast(keysLayoutLongName(next), ui.now());
            break;
        }
        case Side::Del: keysAllNotesOff(ui); break;  // panic
        case Side::Backslash: {
            const bool on = m->params[kLatchParam] < 0.5f;
            keysAllNotesOff(ui);
            setParam(ui, *m, kLatchParam, on ? 1.f : 0.f);
            ui.toast(on ? "LATCH ON" : "LATCH OFF", ui.now());
            break;
        }
        default: break;
    }
}

}  // namespace

// The play page is the exception to the do-it rule: every grid key plays a note (raw keys).
bool keysPlayAction(UiController& ui, const Action& a) {
    playKey(ui, a.key);
    return false;
}
