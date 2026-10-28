#include "tilt_filter.h"
#include <cmath>

namespace {
constexpr float kRadToDeg = 57.29578f;
constexpr float kSmoothMs = 30.f;
}  // namespace

void TiltFilter::push(float ax, float ay, float az, bool fresh, float dtMs) {
    if (!fresh) return;
    const float g2 = ax * ax + ay * ay + az * az;
    if (g2 < 0.25f || g2 > 2.25f) return;
    const TiltAngles t = {atan2f(ax, sqrtf(ay * ay + az * az)) * kRadToDeg,
                          atan2f(ay, sqrtf(ax * ax + az * az)) * kRadToDeg};
    if (!valid_) {
        a_ = t;
        valid_ = true;
        return;
    }
    const float k = 1.f - expf(-(dtMs > 0.f ? dtMs : 0.f) / kSmoothMs);
    a_.roll += k * (t.roll - a_.roll);
    a_.pitch += k * (t.pitch - a_.pitch);
}
