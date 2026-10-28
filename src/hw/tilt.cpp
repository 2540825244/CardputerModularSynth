#include "tilt.h"
#include <M5Unified.h>

namespace {
TiltFilter g_filter;
uint32_t g_lastMs = 0;
}  // namespace

void tiltPoll(uint32_t nowMs) {
    if (!M5.Imu.isEnabled()) return;
    float ax, ay, az;
    // getAccel returns false when the IMU has no new sample since the last read; the values are
    // then stale, not zero, and must not be taken as a reading.
    const bool fresh = M5.Imu.getAccel(&ax, &ay, &az);
    if (!fresh) return;
    // Owner check 2026-10-28: both axes read opposite to the expected direction on the
    // Cardputer-Adv, so x and y are negated here (z keeps gravity's sign).
    g_filter.push(-ax, -ay, az, true, (float)(nowMs - g_lastMs));
    g_lastMs = nowMs;
}

TiltAngles tiltAngles() { return g_filter.angles(); }
bool tiltValid() { return g_filter.valid(); }
