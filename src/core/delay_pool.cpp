#include "delay_pool.h"
#include <cstring>

static int16_t g_slots[kDelaySlots][kDelaySlotSamples];
static bool g_used[kDelaySlots];

int16_t* delayPoolAcquire() {
    for (int i = 0; i < kDelaySlots; i++) {
        if (!g_used[i]) {
            g_used[i] = true;
            memset(g_slots[i], 0, sizeof(g_slots[i]));
            return g_slots[i];
        }
    }
    return nullptr;
}

void delayPoolRelease(int16_t* slot) {
    for (int i = 0; i < kDelaySlots; i++)
        if (g_slots[i] == slot) g_used[i] = false;
}

int delayPoolFree() {
    int n = 0;
    for (int i = 0; i < kDelaySlots; i++) n += !g_used[i];
    return n;
}
