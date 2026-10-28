#pragma once
#include <cstdint>
#include "ui/tilt_filter.h"

// BMI270 via M5.Imu, core 0 only. Call tiltPoll every UI loop; the IMU's data rate is lower than
// the loop rate, so most polls see no new sample and keep the last angles.
void tiltPoll(uint32_t nowMs);
TiltAngles tiltAngles();  // last filtered angles; {0, 0} until tiltValid()
bool tiltValid();
