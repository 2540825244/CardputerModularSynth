#pragma once
#include "../dsp_util.h"

// Which of 3 positions MUX/DMX select, per sample (audio thread).
// CV mode: index = (pos + floor(clamp(sel, 0, 4.999) / 5 * 3)) % 3, with +-50 mV hysteresis.
// STEP mode: each rising SEL gate advances a count; index = (pos + count) % 3.
// sel == nullptr (unpatched): index = pos.
struct Selector {
    int index(int pos, bool step, const float* sel, int i) {
        pos = pos < 0 ? 0 : (pos > 2 ? 2 : pos);
        if (!sel) return pos;
        if (step) {
            gate_.process(sel[i]);
            if (gate_.rose) count_ = (count_ + 1) % 3;
            return (pos + count_) % 3;
        }
        float v = sel[i];
        v = v < 0.f ? 0.f : (v > 4.999f ? 4.999f : v);
        const float x = v * (3.f / 5.f);  // 0..3, one unit per position
        const int c = (int)x;
        // +-50 mV hysteresis at each boundary, so a slow or noisy CV switches once.
        constexpr float kBand = 0.05f * (3.f / 5.f);
        if (last_ < 0 || (c > last_ && x >= c + kBand) || (c < last_ && x < last_ - kBand)) last_ = c;
        return (pos + last_) % 3;
    }

   private:
    GateDetector gate_;
    int count_ = 0;
    int last_ = -1;  // CV mode: current band, -1 before the first sample
};

// Shared parameters of MUX and DMX.
namespace selector {
enum { kPos, kMode };
extern const ParamSpec kParams[2];
}  // namespace selector
