#pragma once
#include <cstdint>
#include "ui/tilt_filter.h"

// BMI270 via M5.Imu, core 0 only. Each tiltPoll is an I2C transfer: main.cpp calls it at <= 50 Hz
// while the tilt is in use, else 4 Hz. A poll that sees no new IMU sample keeps the last angles.
void tiltPoll(uint32_t nowMs);
TiltAngles tiltAngles();  // last filtered angles; {0, 0} until tiltValid()
bool tiltValid();
