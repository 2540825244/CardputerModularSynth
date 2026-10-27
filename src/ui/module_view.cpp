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

void doUnpatch(Patch& p, void* c) {
    const PinEdit* e = static_cast<PinEdit*>(c);
    p.removeCablesAtPin(e->id, e->isOutput, e->pin);
}

bool setValue(UiController& ui, const ModuleRec& m, CellRef c, float v) {
    const bool amount = c.kind == CellKind::Out;
    const ParamSpec& spec = amount ? kAmountSpec : m.type->params[c.index];
    v = paramClamp(spec, v);
    ValueEdit e{m.id, c.index, amount, v};
    ui.edit(doSetValue, &e, paramKey(m.id, c.index, amount));
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

void moduleViewKey(UiController& ui, const KeyEvent& e) {
    UiState& s = ui.mut();
    const ModuleRec* m = ui.patch().find(s.openId);
    if (!m) {
        s.view = View::Patch;
        return;
    }
    if (e.type != KeyEvent::Down) return;
    const CellRef sel = selected(s, *m->type);
    const bool editable = sel.kind == CellKind::Param || sel.kind == CellKind::Out;

    if (e.grid) {
        if (e.row == 0) {
            if (editable) setValue(ui, *m, sel, paramCoarse(specOf(*m, sel), e.col));
        } else {
            s.modCell = (int8_t)cellOf(e);
        }
        return;
    }
    switch (e.side) {
        case Side::Ctrl:
        case Side::Space:
            if (editable) {
                const ParamSpec& sp = specOf(*m, sel);
                setValue(ui, *m, sel, paramFine(sp, currentValue(*m, sel), e.side == Side::Space ? 1 : -1));
            }
            break;
        case Side::Tab:
            s.modCell = (int8_t)nextEditable(*m->type, s.modCell < 0 ? 0 : s.modCell);
            break;
        case Side::Enter:
            if (sel.kind == CellKind::Param) {
                const ParamSpec& sp = m->type->params[sel.index];
                if (sp.curve == Curve::Stepped) {
                    float v = m->params[sel.index] + 1.f;
                    if (v > sp.max) v = sp.min;
                    setValue(ui, *m, sel, v);
                }
            }
            break;
        case Side::Del:
            if (sel.kind == CellKind::Param) {
                setValue(ui, *m, sel, m->type->params[sel.index].def);
            } else if (sel.kind == CellKind::In || sel.kind == CellKind::Out) {
                PinEdit pe{m->id, sel.kind == CellKind::Out, sel.index};
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
