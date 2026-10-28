#pragma once
#include "../dsp_util.h"

// Which of 3 positions MUX/DMX select, per sample (audio thread).
// CV mode: index = (pos + floor(clamp(sel, 0, 4.999) / 5 * 3)) % 3.
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
        return (pos + (int)(v * (3.f / 5.f))) % 3;
    }

   private:
    GateDetector gate_;
    int count_ = 0;
};

// Shared parameters of MUX and DMX.
namespace selector {
enum { kPos, kMode };
extern const ParamSpec kParams[2];
}  // namespace selector
