#include "tilt.h"
#include <M5Unified.h>
#include <math.h>

float tiltRollDegrees() {
    float ax, ay, az;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) return 0.f;
    return atan2f(ax, az) * 57.29578f;
}
