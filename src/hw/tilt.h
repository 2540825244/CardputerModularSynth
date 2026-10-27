#pragma once

// Roll in degrees from the accelerometer (BMI270 via M5.Imu): rotation about the device's long
// axis, i.e. tilting the left edge down or up. Core 0 only. Returns 0 if the IMU is unavailable.
float tiltRollDegrees();
