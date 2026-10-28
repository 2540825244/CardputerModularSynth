#include "mux.h"
#include "selector.h"

namespace selector {
const char* const kPosNames[] = {"A", "B", "C"};
const char* const kModeNames[] = {"CV", "STP"};
const ParamSpec kParams[2] = {
    {"pos", "POS", 0.f, 2.f, 0.f, Curve::Stepped, Unit::Enum, kPosNames},
    {"mode", "MOD", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kModeNames},
};
}  // namespace selector

namespace {

enum { kInSel = 3 };

const char* const kInKeys[] = {"a", "b", "c", "sel"};
const char* const kInLabels[] = {"A", "B", "C", "SEL"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

// 3:1 switch. Each input fades in or out over 2 ms, so switching never clicks.
class Mux : public Module {
   public:
    Mux() : Module(kMuxType) {
        for (Slew& g : gain_) g.setTimeMs(2.f, kSampleRate);
        gain_[0].y = 1.f;
    }

    void process(const float* const* ins, const bool* connected) override {
        const int pos = (int)(param(selector::kPos) + 0.5f);
        const bool step = param(selector::kMode) >= 0.5f;
        const float* sel = connected[kInSel] ? ins[kInSel] : nullptr;
        float* o = out(0);
        int idx = 0;
        for (int i = 0; i < kBlock; i++) {
            idx = sel_.index(pos, step, sel, i);
            float y = 0.f;
            for (int k = 0; k < 3; k++) {
                const float g = gain_[k].process(k == idx ? 1.f : 0.f);
                if (connected[k]) y += ins[k][i] * g;
            }
            o[i] = y;
        }
        setMonitor(0, (float)idx);
    }

   private:
    Selector sel_;
    Slew gain_[3];
};

Module* create() { return new Mux(); }

}  // namespace

const ModuleTypeInfo kMuxType = {
    "mux", "MUX", "3:1 switch, SEL by CV or steps", Category::Processor, Icon::Mux, Drawing::None,
    2, selector::kParams, 4, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    40, sizeof(Mux) + kBlock * sizeof(float), 0, create};
