#pragma once

struct TiltAngles {
    float roll, pitch;  // degrees, each in [-90, 90]
};

// Accelerometer -> tilt angles. Pure, so it builds natively. roll is rotation about the device's
// long axis, pitch about the short one; both are measured against the gravity vector's other two
// components, so neither swings wildly when the device is held upright.
class TiltFilter {
   public:
    // fresh = false: the IMU had no new sample; nothing changes. Samples with |a| outside
    // [0.5 g, 1.5 g] (shake, free fall) are not a tilt and are ignored.
    void push(float ax, float ay, float az, bool fresh, float dtMs);
    TiltAngles angles() const { return a_; }
    bool valid() const { return valid_; }  // at least one accepted sample

   private:
    TiltAngles a_ = {0.f, 0.f};
    bool valid_ = false;
};
