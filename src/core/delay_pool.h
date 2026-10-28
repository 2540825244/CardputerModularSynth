#pragma once
#include <cstdint>

// Big int16 lines for DLY and REV (REV's plate needs 45,945 samples of one slot). One slot is
// static .bss, so the first DLY or REV always fits; the second comes from the heap while it is
// placed, leaving that RAM to the rest of the patch otherwise.
constexpr int kDelaySlots = 2;
constexpr int kStaticDelaySlots = 1;
constexpr int kDelaySlotSamples = 48000;  // 1.0 s at 48 kHz, int16 -> 96 KB per slot
constexpr uint32_t kDelaySlotBytes = kDelaySlotSamples * sizeof(int16_t);

int16_t* delayPoolAcquire();  // UI thread; nullptr if all slots in use or no heap; zeroes the slot
void delayPoolRelease(int16_t* slot);
int delayPoolFree();
int delayPoolStaticFree();  // a create now would not need the heap slot
// Heap bytes a module taking `slots` more slots needs when `used` are already taken.
inline uint32_t delayPoolHeapBytes(int used, int slots) {
    auto heap = [](int n) { return n > kStaticDelaySlots ? n - kStaticDelaySlots : 0; };
    return (uint32_t)(heap(used + slots) - heap(used)) * kDelaySlotBytes;
}
