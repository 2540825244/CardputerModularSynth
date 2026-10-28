#include "dmx.h"
#include "selector.h"

namespace {

enum { kInIn, kInSel };

const char* const kInKeys[] = {"in", "sel"};
const char* const kInLabels[] = {"IN", "SEL"};
const char* const kOutKeys[] = {"a", "b", "c"};
const char* const kOutLabels[] = {"A", "B", "C"};

// 1:3 switch: IN goes to the selected output, the others are 0 V; 2 ms fades.
class Dmx : public Module {
   public:
    Dmx() : Module(kDmxType) {
        for (Slew& g : gain_) g.setTimeMs(2.f, kSampleRate);
        gain_[0].y = 1.f;
    }

    void process(const float* const* ins, const bool* connected) override {
        const int pos = (int)(param(selector::kPos) + 0.5f);
        const bool step = param(selector::kMode) >= 0.5f;
        const float* in = connected[kInIn] ? ins[kInIn] : nullptr;
        const float* sel = connected[kInSel] ? ins[kInSel] : nullptr;
        int idx = 0;
        for (int i = 0; i < kBlock; i++) {
            idx = sel_.index(pos, step, sel, i);
            const float x = in ? in[i] : 0.f;
            for (int k = 0; k < 3; k++) out(k)[i] = x * gain_[k].process(k == idx ? 1.f : 0.f);
        }
        setMonitor(0, (float)idx);
    }

   private:
    Selector sel_;
    Slew gain_[3];
};

Module* create() { return new Dmx(); }

}  // namespace

const ModuleTypeInfo kDmxType = {
    "dmx", "DMX", "1-to-3 switch", Category::Utility, Icon::Dmx, Drawing::None,
    2, selector::kParams, 2, kInKeys, kInLabels, 3, kOutKeys, kOutLabels,
    40, sizeof(Dmx) + 3 * kBlock * sizeof(float), 0, create};
