#include "key_input.h"

namespace {
// Maps a physical position to a key index; -1 if out of range.
int indexOf(KeyPos p, int cols, int gridKeys) {
    if (p.y < 0 || p.y > 3 || p.x < 0 || p.x > 13) return -1;
    if (p.x == 0) return gridKeys + p.y;
    if (p.x == 13) return gridKeys + 4 + p.y;
    return p.y * cols + (p.x - 1);
}
}  // namespace

void KeyDecoder::update(const KeyPos* pressed, int n, uint32_t nowMs, KeyEvent* out, int* outCount, int outMax) {
    bool now[kKeys] = {};
    for (int i = 0; i < n; i++) {
        int k = indexOf(pressed[i], kCols, kGridKeys);
        if (k >= 0) now[k] = true;
    }
    int c = 0;
    auto emit = [&](int k, KeyEvent::Type t) {
        if (c >= outMax) return;
        KeyEvent e{};
        e.type = t;
        e.grid = k < kGridKeys;
        if (e.grid) {
            e.col = static_cast<uint8_t>(k % kCols);
            e.row = static_cast<uint8_t>(k / kCols);
            e.side = Side::Esc;
        } else {
            e.col = e.row = 0;
            e.side = static_cast<Side>(k - kGridKeys);
        }
        out[c++] = e;
    };
    for (int k = 0; k < kKeys; k++) {
        if (now[k] && !held_[k]) {
            downAt_[k] = nowMs;
            holdFired_[k] = false;
            emit(k, KeyEvent::Down);
        } else if (!now[k] && held_[k]) {
            emit(k, KeyEvent::Up);
        } else if (now[k] && !holdFired_[k] && nowMs - downAt_[k] >= holdMs_) {
            holdFired_[k] = true;
            emit(k, KeyEvent::Hold);
        }
        held_[k] = now[k];
    }
    *outCount = c;
}
