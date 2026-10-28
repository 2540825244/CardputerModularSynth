#pragma once
#include "../module.h"

extern const ModuleTypeInfo kTiltType;

// Core 0 (UI loop), after each IMU poll: the latest device tilt in degrees. Every TILT instance
// reads it on the audio thread. Relaxed atomics: the values are independent and only drift slowly.
void tiltPublish(float rollDeg, float pitchDeg);
