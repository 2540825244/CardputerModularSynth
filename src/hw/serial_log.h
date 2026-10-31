#pragma once
#include <Arduino.h>

// Diagnostics on the USB serial (status, heap, measured module costs). Only in builds with
// -DMODSYNTH_SERIAL_LOG (env:cardputer, for tools/dev/listen.py); the release build (env:release)
// prints nothing, and the arguments are not evaluated.
#ifdef MODSYNTH_SERIAL_LOG
#define LOGF(...) Serial.printf(__VA_ARGS__)
#define LOGLN(s) Serial.println(s)
#else
#define LOGF(...) \
    do {          \
    } while (0)
#define LOGLN(s) \
    do {         \
    } while (0)
#endif
