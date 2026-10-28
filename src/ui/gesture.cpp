#include "gesture.h"
#include "core/units.h"

namespace {
Action make(Act a, int cell, const KeyEvent& e) { return Action{a, (int8_t)cell, e}; }
}  // namespace

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

int GestureMapper::map(const KeyEvent& e, bool fnHeld, bool ctrlHeld, bool raw, Action* out) {
    const int cell = e.grid ? e.row * kGridCols + e.col : -1;
    if (e.grid && (cell < 0 || cell >= kCells)) return 0;
    if (e.grid) held_[cell] = e.type != KeyEvent::Up;
    if (raw) {
        if (e.grid) activated_[cell] = e.type != KeyEvent::Up;  // a held note never turns into do-it
        enterDown_ = enterSwept_ = false;
        out[0] = make(Act::Key, cell, e);
        return 1;
    }
    if (e.grid) {
        switch (e.type) {
            case KeyEvent::Down:
                out[0] = make(Act::Select, cell, e);
                activated_[cell] = fnHeld;
                if (!fnHeld) return 1;
                out[1] = make(Act::Activate, cell, e);
                return 2;
            case KeyEvent::Hold:
                if (activated_[cell] || ctrlHeld) return 0;
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
    return 1;
}
