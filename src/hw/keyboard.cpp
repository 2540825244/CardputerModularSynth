#include "keyboard.h"
#include <M5Cardputer.h>

int keyboardPoll(KeyPos* out, int max) {
    for (int k = 0; k < 8; k++) M5Cardputer.update();
    int n = 0;
    for (auto& p : M5Cardputer.Keyboard.keyList()) {
        if (n < max) out[n++] = {(int8_t)p.x, (int8_t)p.y};
    }
    return n;
}
