// modsynth: MODSYNTH_IRAM puts a per-sample hot function in IRAM on the device. Executing from
// flash, a 32-byte cache line costs ~380 cycles to refill (DIO flash) and the UI on the other core
// keeps evicting the 16 KB instruction cache; in IRAM the code never misses.
#pragma once
#if defined(ESP_PLATFORM)
#include <esp_attr.h>
#define MODSYNTH_IRAM IRAM_ATTR
#else
#define MODSYNTH_IRAM
#endif
