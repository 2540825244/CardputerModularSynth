#include "gesture.h"
#include "core/units.h"

namespace {
Action make(Act a, int cell, const KeyEvent& e, int from = -1) { return Action{a, (int8_t)cell, e, (int8_t)from}; }
int modIndex(Side s) { return s == Side::Fn ? 0 : (s == Side::Ctrl ? 1 : -1); }
}  // namespace

int GestureMapper::heldFree(int except) const {
    for (int c = 0; c < kCells; c++)
        if (c != except && held_[c] && !activated_[c]) return c;
    return -1;
}

void GestureMapper::reset() {
    for (int c = 0; c < kCells; c++) activated_[c] = held_[c];
    enterDown_ = enterSwept_ = false;
}

void GestureMapper::consume(const KeyEvent& e) {
    if (e.grid) {
        const int cell = e.row * kGridCols + e.col;
        if (cell >= 0 && cell < kCells) activated_[cell] = true;
    } else if (e.side == Side::Enter) {
        enterDown_ = enterSwept_ = false;
    }
}

int GestureMapper::map(const KeyEvent& e, bool fnHeld, bool holdBlocked, bool raw, Action* out) {
    const int cell = e.grid ? e.row * kGridCols + e.col : -1;
    if (e.grid && (cell < 0 || cell >= kCells)) return 0;
    const bool wasHeld = e.grid && held_[cell];
    if (e.grid) held_[cell] = e.type != KeyEvent::Up;
    // Anything pressed while fn/ctrl is down uses that press up (it is no longer a lone tap).
    if (e.type == KeyEvent::Down && (e.grid || modIndex(e.side) < 0))
        for (int i = 0; i < 2; i++) modUsed_[i] = true;
    if (raw) {
        if (e.grid) activated_[cell] = e.type != KeyEvent::Up;  // a held note never turns into do-it
        enterDown_ = enterSwept_ = false;
        out[0] = make(Act::Key, cell, e);
        return 1;
    }
    if (e.grid) {
        switch (e.type) {
            case KeyEvent::Down: {
                out[0] = make(Act::Select, cell, e);
                const int from = wasHeld ? -1 : heldFree(cell);
                if (from >= 0) {
                    activated_[from] = activated_[cell] = true;
                    out[1] = make(Act::Chord, cell, e, from);
                    return 2;
                }
                activated_[cell] = fnHeld;
                if (!fnHeld) return 1;
                out[1] = make(Act::Activate, cell, e);
                return 2;
            }
            case KeyEvent::Hold:
                if (activated_[cell] || holdBlocked) return 0;
                activated_[cell] = true;
                out[0] = make(Act::Activate, cell, e);
                return 1;
            case KeyEvent::Up:
                activated_[cell] = false;
                out[0] = make(Act::Key, cell, e);
                return 1;
        }
        return 0;
    }
    if (e.side == Side::Enter) {
        switch (e.type) {
            case KeyEvent::Down:
                enterDown_ = true;
                enterSwept_ = false;
                return 0;
            case KeyEvent::Hold:
                if (!enterDown_ || enterSwept_) return 0;
                enterSwept_ = true;
                out[0] = make(Act::Sweep, -1, e);
                return 1;
            case KeyEvent::Up: {
                if (!enterDown_) return 0;
                const bool swept = enterSwept_;
                enterDown_ = enterSwept_ = false;
                out[0] = make(swept ? Act::SweepEnd : Act::Activate, -1, e);
                return 1;
            }
        }
        return 0;
    }
    out[0] = make(Act::Key, -1, e);
    const int mi = modIndex(e.side);
    if (mi < 0) return 1;
    if (e.type == KeyEvent::Down) {
        modDown_[mi] = true;
        bool anyGrid = false;  // a same-frame chord delivers the grid Down first
        for (int k = 0; k < kCells; k++) anyGrid |= held_[k];
        modUsed_[mi] = anyGrid;
    } else if (e.type == KeyEvent::Hold) {
        modUsed_[mi] = true;
    } else {
        const bool tap = modDown_[mi] && !modUsed_[mi];
        modDown_[mi] = false;
        if (tap) {
            out[1] = make(Act::SideTap, -1, e);
            return 2;
        }
    }
    return 1;
}
