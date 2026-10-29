#include "atn.h"
#include "../dsp_util.h"

namespace {

enum { kAmt, kOfs };

const ParamSpec kParams[] = {
    {"amt", "AMT", -1.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"ofs", "OFS", -5.f, 5.f, 0.f, Curve::Lin, Unit::Volts, nullptr},
};
const char* const kInKeys[] = {"in"};
const char* const kInLabels[] = {"IN"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

// out = in x AMT + OFS. With nothing patched in it is a DC source.
class Atn : public Module {
   public:
    Atn() : Module(kAtnType) {
        amt_.setTimeMs(2.f, kSampleRate);
        ofs_.setTimeMs(2.f, kSampleRate);
        amt_.y = 1.f;
    }

    void process(const float* const* ins, const bool* connected) override {
        const float amt = param(kAmt), ofs = param(kOfs);
        const float* in = connected[0] ? ins[0] : nullptr;
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float a = amt_.process(amt), b = ofs_.process(ofs);
            o[i] = (in ? in[i] * a : 0.f) + b;
        }
    }

   private:
    Slew amt_, ofs_;
};

Module* create() { return new Atn(); }

}  // namespace

const ModuleTypeInfo kAtnType = {
    "atn", "ATN", "Attenuverter + offset (DC)", Category::Utility, Icon::Atn, Drawing::None,
    2, kParams, 1, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    120, sizeof(Atn) + kBlock * sizeof(float), 0, create};
