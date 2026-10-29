#include "dsp_util.h"
#include <cmath>

void Slew::setTimeMs(float ms, float fs) {
    if (ms <= 0.f) {
        coef = 1.f;
        return;
    }
    coef = 1.f - expf(-1.f / (ms * 0.001f * fs));  // UI thread only
}
