#pragma once
#include <cstdint>

constexpr int kDelaySlots = 2;            // 1 if Task 2's largest free block < 260 KB
constexpr int kDelaySlotSamples = 48000;  // 1.0 s at 48 kHz, int16 -> 96 KB per slot, static .bss

int16_t* delayPoolAcquire();  // UI thread; nullptr if all slots in use; zeroes the slot
void delayPoolRelease(int16_t* slot);
int delayPoolFree();
