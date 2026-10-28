#include "layout.h"
#include "views.h"

namespace {

struct ValueEdit {
    uint16_t id;
    uint8_t idx;
    bool amount;
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
    if (v->amount) m->outAmount[v->idx] = v->value;
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

bool setValue(UiController& ui, const ModuleRec& m, CellRef c, float v) {
    const bool amount = c.kind == CellKind::Out;
    const ParamSpec& spec = amount ? kAmountSpec : m.type->params[c.index];
    v = paramClamp(spec, v);
    ValueEdit e{m.id, c.index, amount, v};
    ui.editValue(doSetValue, &e, paramKey(m.id, c.index, amount));
    if (amount) ui.engine().setOutAmount(m.id, c.index, v);
    else ui.engine().setParam(m.id, c.index, v);
    return true;
}

CellRef selected(const UiState& s, const ModuleTypeInfo& t) {
    if (s.modCell < 0) return {CellKind::Empty, 0};
    return moduleCell(t, (uint8_t)(s.modCell / kGridCols), (uint8_t)(s.modCell % kGridCols));
}

float currentValue(const ModuleRec& m, CellRef c) {
    return c.kind == CellKind::Out ? m.outAmount[c.index] : m.params[c.index];
}

const ParamSpec& specOf(const ModuleRec& m, CellRef c) {
    return c.kind == CellKind::Out ? kAmountSpec : m.type->params[c.index];
}

// Tab: next Param/Out cell after the cursor, in cell order, wrapping.
int nextEditable(const ModuleTypeInfo& t, int from) {
    for (int step = 1; step <= kCellsPerPage; step++) {
        const int c = (from + step + kCellsPerPage) % kCellsPerPage;
        const CellRef r = moduleCell(t, (uint8_t)(c / kGridCols), (uint8_t)(c % kGridCols));
        if (r.kind == CellKind::Param || r.kind == CellKind::Out) return c;
    }
    return from;
}

}  // namespace

namespace {

// Do-it on a cell: a pin carries a cable, a stepped param steps to its next option.
void activate(UiController& ui, const ModuleRec& m, int cell) {
    if (cell < kGridCols) return;  // the value strip has no do-it
    const CellRef c = moduleCell(*m.type, (uint8_t)(cell / kGridCols), (uint8_t)(cell % kGridCols));
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

void sideKey(UiController& ui, const ModuleRec& m, const KeyEvent& e) {
    UiState& s = ui.mut();
    if (e.type != KeyEvent::Down) return;
    const CellRef sel = selected(s, *m.type);
    const bool editable = sel.kind == CellKind::Param || sel.kind == CellKind::Out;
    switch (e.side) {
        case Side::Ctrl:
        case Side::Space:
            if (editable) {
                const ParamSpec& sp = specOf(m, sel);
                setValue(ui, m, sel, paramFine(sp, currentValue(m, sel), e.side == Side::Space ? 1 : -1));
            }
            break;
        case Side::Tab:
            ui.endTilt();
            s.modCell = (int8_t)nextEditable(*m.type, s.modCell < 0 ? 0 : s.modCell);
            break;
        case Side::Del:
            if (sel.kind == CellKind::Param) {
                setValue(ui, m, sel, m.type->params[sel.index].def);
            } else if (sel.kind == CellKind::In || sel.kind == CellKind::Out) {
                PinEdit pe{m.id, sel.kind == CellKind::Out, sel.index};
                ui.edit(doUnpatch, &pe);
            }
            break;
        case Side::Backslash:
            ui.undo();
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
    switch (a.act) {
        case Act::Select:
            if (ui.tiltActive() && a.cell >= kGridCols) ui.endTilt();  // the cursor leaves the swept param
            if (a.cell < kGridCols) {  // value strip: coarse value for the selected cell
                const CellRef sel = selected(s, *m->type);
                if (sel.kind == CellKind::Param || sel.kind == CellKind::Out)
                    setValue(ui, *m, sel, paramCoarse(specOf(*m, sel), a.cell));
            } else {
                s.modCell = a.cell;
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
        case Act::Key:
            if (!a.key.grid) sideKey(ui, *m, a.key);
            break;
    }
    return false;
}
