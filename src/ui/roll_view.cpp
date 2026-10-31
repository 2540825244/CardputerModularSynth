// v1.5: the ROL roll page. Step entry on the 12 x 4 grid: the number row picks a note length
// (1..16 steps), REST, TIE and the keyboard octave; rows 2..4 are the scale from ROOT (bottom row
// an octave below the middle one) and write a note of that length at the cursor, play it, and
// move the cursor past it. See the side icons for the side keys.
#include <cstring>
#include "core/modules/seq2.h"
#include "core/scales.h"
#include "core/transport.h"
#include "views.h"

using namespace roll;

namespace {
const uint8_t kLens[8] = {1, 2, 3, 4, 6, 8, 12, 16};

struct WordsEdit {
    uint16_t id;
    float w[kWords];
};
void doWords(Patch& p, void* c) {
    const WordsEdit* e = static_cast<WordsEdit*>(c);
    if (ModuleRec* m = p.find(e->id)) memcpy(m->params + kN0, e->w, sizeof e->w);
}
// One undo step per edit, however many words it touched.
void commit(UiController& ui, const ModuleRec& m, const float* w) {
    if (!memcmp(w, m.params + kN0, sizeof(float) * kWords)) return;
    WordsEdit e{m.id, {}};
    memcpy(e.w, w, sizeof e.w);
    ui.editValue(doWords, &e, 0);
    for (int i = 0; i < kWords; i++) ui.engine().setParam(m.id, (uint8_t)(kN0 + i), e.w[i]);
}

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

RollModule* instanceOf(UiController& ui, const ModuleRec& m) {
    return m.type == &kRolType ? static_cast<RollModule*>(ui.engine().instance(m.id)) : nullptr;
}

int patternLen(const ModuleRec& m) {
    const int len = (int)m.params[kLen];
    return len < 1 ? 1 : (len > kSteps ? kSteps : len);
}

void rollKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    const ModuleRec* m = ui.patch().find(s.openId);
    if (!m) {
        s.view = View::Patch;
        return;
    }
    RollModule* r = instanceOf(ui, *m);
    const int len = patternLen(*m);
    if (s.rollCursor >= len) s.rollCursor = 0;
    if (s.rollLen > 7) s.rollLen = 2;
    const int cur = s.rollCursor, n = kLens[s.rollLen];
    float w[kWords];
    memcpy(w, m->params + kN0, sizeof w);
    if (e.grid) {
        if (e.type == KeyEvent::Hold) return;
        if (e.row == 0) {  // lengths, REST, TIE, octave
            if (e.type != KeyEvent::Down) return;
            if (e.col < 8) {
                if (ui.fnHeld()) {  // fn+length: resize the note under the cursor
                    resize(w, len, cur, kLens[e.col]);
                    commit(ui, *m, w);
                } else {
                    s.rollLen = (uint8_t)e.col;
                }
            } else if (e.col == 8 || e.col == 9) {
                if (e.col == 8) rest(w, len, cur, n);
                else extend(w, len, cur, n);
                commit(ui, *m, w);
                s.rollCursor = (uint8_t)((cur + n) % len);
            } else {
                setParam(ui, *m, kKbd, m->params[kKbd] + (e.col == 11 ? 1.f : -1.f));
            }
            return;
        }
        const int semis = rollKeySemis(*m, e.row, e.col);
        const uint64_t bit = 1ull << (e.row * kGridCols + e.col);
        if (e.type == KeyEvent::Down) {
            s.rollHeld |= bit;
            s.rollSemis = (int8_t)semis;
            if (r) r->audition(semis);
            if (!ui.fnHeld()) {  // fn+key only previews
                writeNote(w, len, cur, n, semis);
                commit(ui, *m, w);
                s.rollCursor = (uint8_t)((cur + n) % len);
            }
        } else {
            if (!(s.rollHeld & bit)) return;  // pressed before this page opened
            s.rollHeld &= ~bit;
            if (!s.rollHeld && r) r->auditionOff();
        }
        return;
    }
    if (e.type != KeyEvent::Down) return;
    const bool fn = ui.fnHeld();
    switch (e.side) {
        case Side::Esc:
            rollLeavePage(ui);
            s.view = View::Patch;
            break;
        case Side::Ctrl:
        case Side::Space: {
            const int by = (fn ? stepsPerBeat((int)m->params[kRes]) : 1) * (e.side == Side::Space ? 1 : -1);
            s.rollCursor = (uint8_t)(((cur + by) % len + len) % len);
            break;
        }
        case Side::Del:
            if (fn) {
                rest(w, kSteps, 0, kSteps);
                ui.toast("CLEARED", ui.now());
            } else {
                erase(w, cur);
            }
            commit(ui, *m, w);
            break;
        case Side::Enter:
            if (fn) {  // the global play / stop, as Space in the patch view
                transport::toggle();
                ui.toast(transport::running() ? "PLAY" : "STOP", ui.now());
            } else {
                const bool on = m->params[kRun] < 0.5f;
                setParam(ui, *m, kRun, on ? 1.f : 0.f);
                ui.toast(on ? "RUN ON" : "RUN OFF", ui.now());
            }
            break;
        case Side::Backslash:
            ui.undoOrRedo();
            if (!ui.patch().find(s.openId)) s.view = View::Patch;
            break;
        default: break;
    }
}
}  // namespace

int rollLenSteps(int key) { return kLens[key < 0 ? 0 : (key > 7 ? 7 : key)]; }

int rollKeySemis(const ModuleRec& m, int row, int col) {
    const int scale = (int)m.params[kScale], up = 3 - row;  // 0 = the bottom row
    return scaleDegree(scale, (up - 1) * scaleSize(scale) + col) + 12 * (int)m.params[kKbd];
}

void rollLeavePage(UiController& ui) {
    UiState& s = ui.mut();
    s.rollHeld = 0;
    if (const ModuleRec* m = ui.patch().find(s.openId))
        if (RollModule* r = instanceOf(ui, *m)) r->auditionOff();
}

bool rollPlayAction(UiController& ui, const Action& a) {
    if (a.act == Act::Key) rollKey(ui, a.key);
    return false;
}

bool transportOwner(const Patch& p) {
    for (int i = 0; i < p.moduleCount(); i++) {
        const ModuleRec& m = p.moduleAt(i);
        if ((m.type == &kRolType || m.type == &kEvoType) && !m.bypass) return true;
    }
    return false;
}
