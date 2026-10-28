#include "side_icons.h"
#include <cstdio>
#include <cstring>
#include "core/modules/keys.h"
#include "key_input.h"
#include "layout.h"

namespace {

void set(SideIcons& r, Side s, Ic ic, const char* name, bool active = false) {
    const int i = (int)s;
    r.icon[i] = ic;
    r.shown[i] = true;
    r.name[i] = name;
    r.active[i] = active;
}

void pageText(SideIcons& r, int page, int count) {
    if (count > 1) {
        snprintf(r.tabText, sizeof r.tabText, "%d/%d", page + 1, count);
        r.shown[(int)Side::Tab] = true;
        r.name[(int)Side::Tab] = "PAGE";
    }
}

void undo(SideIcons& r, bool fnHeld) {
    if (fnHeld) set(r, Side::Backslash, Ic::Redo, "REDO");
    else set(r, Side::Backslash, Ic::Undo, "UNDO");
}

void patchView(SideIcons& r, const UiState& s, const Patch& p, bool fnHeld) {
    const bool moving = s.moveFromId >= 0;
    const ModuleRec* sel = s.selCell >= 0 ? p.findAt(s.page, (uint8_t)s.selCell) : nullptr;
    if (moving || s.carry.active) set(r, Side::Esc, Ic::Cancel, "CANCEL");
    else set(r, Side::Esc, Ic::File, "FILE");
    pageText(r, s.page, kPages);
    if (s.carry.active) set(r, Side::Fn, Ic::Cable, "DROP", true);
    else if (moving) set(r, Side::Fn, Ic::Copy, "COPY");
    else set(r, Side::Fn, Ic::Plus, "PLACE");
    set(r, Side::Ctrl, Ic::Move, "MOVE", moving);
    if (sel && !moving) set(r, Side::Del, Ic::Trash, "DELETE");
    undo(r, fnHeld);
    if (s.selCell >= 0 && !moving) set(r, Side::Enter, sel ? Ic::Open : Ic::Plus, sel ? "OPEN" : "PLACE");
    for (int i = 0; i < p.moduleCount(); i++)
        if (p.moduleAt(i).type == &kKeysType) set(r, Side::Space, Ic::Play, "PLAY");
}

void modulePage(SideIcons& r, const UiState& s, const ModuleRec& m, bool fnHeld) {
    const ModuleTypeInfo& t = *m.type;
    set(r, Side::Esc, Ic::Back, "BACK");
    pageText(r, s.modPage, modulePageCount(t));
    undo(r, fnHeld);
    if (modulePageKind(t, s.modPage) == PageKind::Play) {  // KEYS play page
        set(r, Side::Ctrl, Ic::Minus, "OCT-");
        set(r, Side::Space, Ic::Plus, "OCT+");
        set(r, Side::Enter, Ic::Layout, "LAYOUT");
        set(r, Side::Del, Ic::Cancel, "PANIC");
        set(r, Side::Backslash, Ic::Latch, "LATCH", m.params[7] >= 0.5f);
        set(r, Side::Fn, Ic::Shift, "PREV");  // fn+enter previous layout, fn+tab previous page
        return;
    }
    set(r, Side::Fn, Ic::Cable, s.carry.active ? "DROP" : "CABLE", s.carry.active);
    const CellRef c = s.modCell >= 0 ? moduleCell(t, s.modPage, (uint8_t)(s.modCell / kGridCols), (uint8_t)(s.modCell % kGridCols))
                                     : CellRef{CellKind::Empty, 0, 0};
    const bool pin = c.kind == CellKind::In || c.kind == CellKind::Out;
    const bool editable = pin || c.kind == CellKind::Param;
    if (editable) {
        set(r, Side::Ctrl, Ic::Minus, "FINE-");
        set(r, Side::Space, Ic::Plus, "FINE+");
        set(r, Side::Del, Ic::Trash, pin ? "UNPTCH" : "RESET");
    }
    if (pin) set(r, Side::Enter, Ic::Cable, s.carry.active ? "DROP" : "CABLE");
    else if (c.kind == CellKind::Bit) set(r, Side::Enter, Ic::Toggle, "STEP");
    else if (c.kind == CellKind::Param)
        set(r, Side::Enter, t.params[c.index].curve == Curve::Stepped ? Ic::Toggle : Ic::Tilt,
            t.params[c.index].curve == Curve::Stepped ? "NEXT" : "TILT");
}

}  // namespace

SideIcons sideIcons(const UiState& s, const Patch& p, bool fnHeld) {
    SideIcons r;
    memset(&r, 0, sizeof r);
    switch (s.view) {
        case View::Patch: patchView(r, s, p, fnHeld); break;
        case View::Module:
            if (const ModuleRec* m = p.find(s.openId)) modulePage(r, s, *m, fnHeld);
            break;
        case View::Place:
            set(r, Side::Esc, Ic::Back, "BACK");
            snprintf(r.tabText, sizeof r.tabText, "%s", placePageName(s.placePage));
            r.shown[(int)Side::Tab] = true;
            r.name[(int)Side::Tab] = "CATEG";
            set(r, Side::Fn, Ic::Plus, "PLACE");  // fn+tile places it at once
            if (s.placeType >= 0) set(r, Side::Enter, Ic::Enter, fnHeld ? "FORCE" : "PLACE", fnHeld);
            break;
        case View::File: {
            static const char* const kPg[] = {"LOAD", "SAVE", "PRESET", "SYS"};
            set(r, Side::Esc, Ic::Back, "BACK");
            snprintf(r.tabText, sizeof r.tabText, "%d/4", s.filePage + 1);
            r.shown[(int)Side::Tab] = true;
            r.name[(int)Side::Tab] = "PAGE";
            undo(r, fnHeld);
            if (s.filePage < 3) set(r, Side::Fn, Ic::Enter, kPg[s.filePage]);  // fn+slot runs it
            else set(r, Side::Fn, Ic::Shift, "PREV");                          // fn+tab only
            if (s.fileSlot >= 0 && s.filePage < 3) set(r, Side::Enter, Ic::Enter, kPg[s.filePage]);
            break;
        }
    }
    if (s.carry.active && s.view != View::Patch && s.view != View::Module)
        set(r, Side::Fn, Ic::Cable, "DROP", true);
    return r;
}

bool helpVisible(const UiState& s, uint32_t now) {
    const int fn = (int)Side::Fn;
    if (!(s.sideHeld & (1u << fn))) return false;
    const uint32_t down = s.sideDownMs[fn];
    if ((int32_t)(now - down) < 600) return false;
    if (s.pressCell >= 0 && (int32_t)(s.pressMs - down) >= 0) return false;
    for (int i = 0; i < 8; i++)
        if (i != fn && (s.sideHeld & (1u << i))) return false;
    for (int i = 0; i < 8; i++)
        if (i != fn && s.sideDownMs[i] && (int32_t)(s.sideDownMs[i] - down) > 0) return false;
    return true;
}

bool sidePressed(const UiState& s, int side, uint32_t now) {
    if (side < 0 || side >= 8) return false;
    if (s.sideHeld & (1u << side)) return true;
    return s.sideDownMs[side] && (int32_t)(now - s.sideDownMs[side]) < 120;
}
