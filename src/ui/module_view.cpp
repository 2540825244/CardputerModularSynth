#include "layout.h"
#include "views.h"

namespace {

struct ValueEdit {
    uint16_t id;
    uint8_t idx;
    uint8_t kind;  // 0 param, 1 output amount, 2 input amount (paramKey kinds)
    float value;
};

struct PinEdit {
    uint16_t id;
    bool isOutput;
    uint8_t pin;
};

void doSetValue(Patch& p, void* c) {
    const ValueEdit* v = static_cast<ValueEdit*>(c);
    ModuleRec* m = p.find(v->id);
    if (!m) return;
    if (v->kind == 1) m->outAmount[v->idx] = v->value;
    else if (v->kind == 2) m->inAmount[v->idx] = v->value;
    else m->params[v->idx] = v->value;
}

struct CableEdit {
    uint16_t from, to;
    uint8_t fromOut, toIn;
};

void doAddCable(Patch& p, void* c) {
    const CableEdit* e = static_cast<CableEdit*>(c);
    p.addCable(e->from, e->fromOut, e->to, e->toIn);
}

// Do-it on a pin: start a carry, or drop it on a pin of the opposite kind.
void pinAction(UiController& ui, const ModuleRec& m, bool isOutput, uint8_t pin) {
    UiState& s = ui.mut();
    const Patch& p = ui.patch();
    if (!s.carry.active) {
        s.carry = {true, m.id, isOutput, pin, p.nextCableColor()};
        return;
    }
    if (s.carry.fromOutput == isOutput) {
        ui.toast(s.carry.fromOutput ? "NEED INPUT" : "NEED OUTPUT", ui.now());
        return;
    }
    CableEdit e = s.carry.fromOutput ? CableEdit{s.carry.id, m.id, s.carry.pin, pin}
                                     : CableEdit{m.id, s.carry.id, pin, s.carry.pin};
    for (int i = 0; i < p.cableCount(); i++) {
        const CableRec& c = p.cableAt(i);
        if (c.fromId == e.from && c.fromOut == e.fromOut && c.toId == e.to && c.toIn == e.toIn) {
            ui.toast("ALREADY PATCHED", ui.now());
            return;
        }
    }
    if (p.cableCount() >= kMaxCables) {
        ui.toast("TOO MANY CABLES", ui.now());
        return;
    }
    ui.edit(doAddCable, &e);
    s.carry.active = false;
}

void doUnpatch(Patch& p, void* c) {
    const PinEdit* e = static_cast<PinEdit*>(c);
    p.removeCablesAtPin(e->id, e->isOutput, e->pin);
}

bool isPin(CellRef c) { return c.kind == CellKind::In || c.kind == CellKind::Out; }
bool isEditable(CellRef c) { return c.kind == CellKind::Param || isPin(c); }

bool setValue(UiController& ui, const ModuleRec& m, CellRef c, float v) {
    const uint8_t kind = c.kind == CellKind::Out ? 1 : (c.kind == CellKind::In ? 2 : 0);
    const ParamSpec& spec = kind ? kAmountSpec : m.type->params[c.index];
    v = paramClamp(spec, v);
    ValueEdit e{m.id, c.index, kind, v};
    ui.editValue(doSetValue, &e, paramKey(m.id, c.index, kind));
    if (kind == 1) ui.engine().setOutAmount(m.id, c.index, v);
    else if (kind == 2) ui.engine().setInAmount(m.id, c.index, v);
    else ui.engine().setParam(m.id, c.index, v);
    return true;
}

CellRef cellAt(const UiState& s, const ModuleTypeInfo& t, int cell) {
    if (cell < 0) return c_;
    return moduleCell(t, s.modPage, (uint8_t)(cell / kGridCols), (uint8_t)(cell % kGridCols));
}

CellRef selected(const UiState& s, const ModuleTypeInfo& t) { return cellAt(s, t, s.modCell); }

float currentValue(const ModuleRec& m, CellRef c) {
    if (c.kind == CellKind::Out) return m.outAmount[c.index];
    if (c.kind == CellKind::In) return m.inAmount[c.index];
    return m.params[c.index];
}

const ParamSpec& specOf(const ModuleRec& m, CellRef c) {
    return isPin(c) ? kAmountSpec : m.type->params[c.index];
}

}  // namespace

namespace {

// Do-it on a cell: a pin carries a cable, a stepped param steps to its next option.
void activate(UiController& ui, const ModuleRec& m, int cell) {
    if (cell < kGridCols) return;  // the value strip has no do-it
    const CellRef c = cellAt(ui.state(), *m.type, cell);
    if (c.kind == CellKind::In || c.kind == CellKind::Out) {
        pinAction(ui, m, c.kind == CellKind::Out, c.index);
    } else if (c.kind == CellKind::Param) {
        const ParamSpec& sp = m.type->params[c.index];
        if (sp.curve != Curve::Stepped) return;
        float v = m.params[c.index] + 1.f;
        if (v > sp.max) v = sp.min;
        setValue(ui, m, c, v);
    }
}

struct BitEdit {
    uint16_t id;
    uint8_t idx;
    float value;
};

void doSetBit(Patch& p, void* c) {
    const BitEdit* b = static_cast<BitEdit*>(c);
    if (ModuleRec* m = p.find(b->id)) m->params[b->idx] = b->value;
}

// A step cell: a tap flips its bit (one undo entry per tap).
void toggleBit(UiController& ui, const ModuleRec& m, CellRef c) {
    const ParamSpec& sp = m.type->params[c.index];
    const uint32_t v = (uint32_t)m.params[c.index] ^ (1u << c.bit);
    BitEdit e{m.id, c.index, paramClamp(sp, (float)v)};
    ui.undoStack().breakCoalescing();
    ui.edit(doSetBit, &e);
}

}  // namespace

// tab / fn+tab: next / previous page. Leaving a Play page releases its notes.
void flipPage(UiController& ui, const ModuleRec& m, int dir) {
    UiState& s = ui.mut();
    const int n = modulePageCount(*m.type);
    ui.endTilt();
    if (n <= 1) return;
    if (ui.onPlayPage()) keysAllNotesOff(ui);
    s.modPage = (uint8_t)((s.modPage + n + dir) % n);
    s.modCell = -1;
}

namespace {

void sideKey(UiController& ui, const ModuleRec& m, const KeyEvent& e) {
    UiState& s = ui.mut();
    if (e.type != KeyEvent::Down) return;
    const CellRef sel = selected(s, *m.type);
    const bool editable = isEditable(sel);
    switch (e.side) {
        case Side::Ctrl:
        case Side::Space:
            if (editable) {
                const ParamSpec& sp = specOf(m, sel);
                setValue(ui, m, sel, paramFine(sp, currentValue(m, sel), e.side == Side::Space ? 1 : -1));
            }
            break;
        case Side::Tab: flipPage(ui, m, ui.fnHeld() ? -1 : 1); break;
        case Side::Del:
            if (sel.kind == CellKind::Param) {
                setValue(ui, m, sel, m.type->params[sel.index].def);
            } else if (sel.kind == CellKind::In || sel.kind == CellKind::Out) {
                PinEdit pe{m.id, sel.kind == CellKind::Out, sel.index};
                ui.edit(doUnpatch, &pe);
            }
            break;
        case Side::Backslash:
            ui.undoOrRedo();
            if (!ui.patch().find(s.openId)) s.view = View::Patch;
            break;
        case Side::Esc: s.view = View::Patch; break;
        default: break;
    }
}

}  // namespace

bool moduleViewAction(UiController& ui, const Action& a) {
    UiState& s = ui.mut();
    const ModuleRec* m = ui.patch().find(s.openId);
    if (!m) {
        s.view = View::Patch;
        return false;
    }
    if (ui.onPlayPage()) {
        if (a.act == Act::Key && !a.key.grid && a.key.type == KeyEvent::Down && a.key.side == Side::Tab) {
            flipPage(ui, *m, ui.fnHeld() ? -1 : 1);
            return false;
        }
        return keysPlayAction(ui, a);
    }
    switch (a.act) {
        case Act::Select:
            ui.endTilt();  // a cursor move or a coarse value ends a sweep (a no-op when none runs)
            if (a.cell < kGridCols) {  // value strip: coarse value for the selected cell
                const CellRef sel = selected(s, *m->type);
                if (isEditable(sel))
                    setValue(ui, *m, sel, paramCoarse(specOf(*m, sel), a.cell));
            } else {
                s.modCell = a.cell;
                const CellRef c = selected(s, *m->type);
                if (c.kind == CellKind::Bit) toggleBit(ui, *m, c);
            }
            break;
        case Act::Activate: activate(ui, *m, a.cell >= 0 ? a.cell : s.modCell); break;
        case Act::Sweep: {
            // Only knobs sweep; a stepped param (LP/HP) steps like a tap of enter.
            const CellRef sel = selected(s, *m->type);
            if (sel.kind == CellKind::Param && m->type->params[sel.index].curve != Curve::Stepped)
                ui.beginTilt(m->id, sel.index);
            else
                activate(ui, *m, s.modCell);
            break;
        }
        case Act::SweepEnd: ui.endTilt(); break;
        case Act::Chord: {
            // Hold a pin, press a pin of the other kind: patch them directly.
            const CellRef from = cellAt(s, *m->type, a.from), to = cellAt(s, *m->type, a.cell);
            const bool fromPin = from.kind == CellKind::In || from.kind == CellKind::Out;
            const bool toPin = to.kind == CellKind::In || to.kind == CellKind::Out;
            if (s.carry.active || !fromPin || !toPin || from.kind == to.kind) break;
            pinAction(ui, *m, from.kind == CellKind::Out, from.index);
            pinAction(ui, *m, to.kind == CellKind::Out, to.index);
            s.modCell = a.cell;
            return true;
        }
        case Act::SideTap: break;
        case Act::Key:
            if (!a.key.grid) sideKey(ui, *m, a.key);
            break;
    }
    return false;
}
