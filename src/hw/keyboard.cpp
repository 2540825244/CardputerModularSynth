#include "keyboard.h"
#include <M5Cardputer.h>
#include <utility/Adafruit_TCA8418/Adafruit_TCA8418.h>
#include <utility/Adafruit_TCA8418/Adafruit_TCA8418_registers.h>

// Our own TCA8418 reader instead of M5Cardputer's. Theirs reads only after a GPIO edge and clears
// its flag after reading INT_STAT: a key event landing between that read and the clear pulls INT
// low with no edge to follow, and the keyboard is dead until reboot (owner, 2026-10-29: "with
// multiple key presses at the same time, the whole thing freezes"). INT is a level (low while the
// FIFO holds events), so read the pin instead, plus a slow I2C check in case the pin misleads.
namespace {
constexpr int kIntPin = 11;               // TCA8418 INT, active low (hardware-spec)
constexpr uint32_t kFallbackMs = 100;     // drain over I2C this often even with INT high
constexpr uint8_t kOverflow = 0x08;       // INT_STAT OVR_FLOW_INT: the 10-event FIFO dropped events

Adafruit_TCA8418 gTca;
KeyPos gHeld[16];
int gHeldN = 0;
uint32_t gLastDrainMs = 0;

// TCA8418 event (1..80 = row * 10 + col + 1) to Cardputer key coordinates (M5Cardputer's remap).
KeyPos toPos(uint8_t code) {
    const int raw = code - 1, r = raw / 10, c = raw % 10;
    return {(int8_t)(r * 2 + (c > 3 ? 1 : 0)), (int8_t)(c % 4)};
}

void apply(uint8_t ev) {
    const uint8_t code = ev & 0x7F;
    if (code < 1 || code > 80) return;  // a GPI event, not a key
    const KeyPos p = toPos(code);
    int at = -1;
    for (int i = 0; i < gHeldN; i++)
        if (gHeld[i].x == p.x && gHeld[i].y == p.y) at = i;
    if (ev & 0x80) {  // press
        if (at < 0 && gHeldN < 16) gHeld[gHeldN++] = p;
    } else if (at >= 0) {
        gHeld[at] = gHeld[--gHeldN];
    }
}
}  // namespace

bool keyboardBegin() {
    if (!gTca.begin()) return false;
    gTca.matrix(7, 8);
    gTca.flush();
    pinMode(kIntPin, INPUT);
    gTca.enableInterrupts();  // drives the INT pin we read; no GPIO interrupt is attached
    // and latch an overflow in INT_STAT (the bit may need its enable to be set at all)
    gTca.writeRegister8(TCA8418_REG_CFG, gTca.readRegister8(TCA8418_REG_CFG) | TCA8418_REG_CFG_OVR_FLOW_IEN);
    return true;
}

int keyboardPoll(KeyPos* out, int max) {
    const uint32_t now = millis();
    if (digitalRead(kIntPin) == LOW || now - gLastDrainMs >= kFallbackMs) {
        gLastDrainMs = now;
        for (int k = 0; k < 16; k++) {  // the FIFO holds 10
            const uint8_t ev = gTca.getEvent();
            if (!ev) break;
            apply(ev);
        }
        // Read after the drain and clear only what was read: an overflow during the drain is
        // seen now or next time, never wiped unseen. Dropped events may include releases: forget
        // what is held rather than keep a stuck key (a key still down reads as up until pressed
        // again).
        const uint8_t st = gTca.readRegister8(TCA8418_REG_INT_STAT);
        if (st & kOverflow) gHeldN = 0;
        if (st) gTca.writeRegister8(TCA8418_REG_INT_STAT, st);
    }
    const int n = gHeldN < max ? gHeldN : max;
    for (int i = 0; i < n; i++) out[i] = gHeld[i];
    return n;
}
