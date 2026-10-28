#include "delay_pool.h"
#include <cstdlib>
#include <cstring>

static int16_t g_static[kStaticDelaySlots][kDelaySlotSamples];
static int16_t* g_slot[kDelaySlots];  // [0, kStaticDelaySlots) point at g_static, the rest are heap
static bool g_used[kDelaySlots];

int16_t* delayPoolAcquire() {
    for (int i = 0; i < kDelaySlots; i++) {
        if (g_used[i]) continue;
        if (i < kStaticDelaySlots) g_slot[i] = g_static[i];
        else if (!(g_slot[i] = static_cast<int16_t*>(malloc(kDelaySlotBytes)))) return nullptr;
        g_used[i] = true;
        memset(g_slot[i], 0, kDelaySlotBytes);
        return g_slot[i];
    }
    return nullptr;
}

void delayPoolRelease(int16_t* slot) {
    for (int i = 0; i < kDelaySlots; i++) {
        if (!g_used[i] || g_slot[i] != slot) continue;
        g_used[i] = false;
        if (i >= kStaticDelaySlots) {
            free(g_slot[i]);
            g_slot[i] = nullptr;
        }
    }
}

int delayPoolFree() {
    int n = 0;
    for (int i = 0; i < kDelaySlots; i++) n += !g_used[i];
    return n;
}

int delayPoolStaticFree() {
    int n = 0;
    for (int i = 0; i < kStaticDelaySlots; i++) n += !g_used[i];
    return n;
}
