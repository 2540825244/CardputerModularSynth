#include "mix.h"
#include "../dsp_util.h"

namespace {

constexpr int kN = 4;

const ParamSpec kParams[] = {
    {"lvl1", "L1", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"lvl2", "L2", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"lvl3", "L3", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"lvl4", "L4", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in1", "in2", "in3", "in4"};
const char* const kInLabels[] = {"IN1", "IN2", "IN3", "IN4"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

class Mix : public Module {
   public:
    Mix() : Module(kMixType) {
        for (Slew& g : gain_) {
            g.setTimeMs(2.f, kSampleRate);
            g.y = 1.f;
        }
    }

    void process(const float* const* ins, const bool* connected) override {
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) o[i] = 0.f;
        for (int n = 0; n < kN; n++) {
            const float level = param(n);
            Slew& g = gain_[n];
            if (!connected[n]) {
                g.y = level;  // nothing to fade: track silently
                continue;
            }
            const float* in = ins[n];
            for (int i = 0; i < kBlock; i++) o[i] += in[i] * g.process(level);
        }
    }

   private:
    Slew gain_[kN];
};

Module* create() { return new Mix(); }

}  // namespace

const ModuleTypeInfo kMixType = {
    "mix", "MIX", "4-input mixer", Category::Utility, Icon::Mix, Drawing::Picture,
    4, kParams, 4, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    230, sizeof(Mix) + kBlock * sizeof(float), 0, create};
