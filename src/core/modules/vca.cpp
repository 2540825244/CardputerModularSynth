#include "vca.h"
#include "../dsp_util.h"

namespace {

enum { kLevel };
enum { kInIn, kInCv };

const ParamSpec kParams[] = {
    {"level", "LVL", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "cv"};
const char* const kInLabels[] = {"IN", "CV"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

class Vca : public Module {
   public:
    Vca() : Module(kVcaType) { gain_.setTimeMs(2.f, kSampleRate); }

    void process(const float* const* ins, const bool* connected) override {
        const float level = param(kLevel);
        const float* in = connected[kInIn] ? ins[kInIn] : nullptr;
        const float* cv = connected[kInCv] ? ins[kInCv] : nullptr;
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            float target = level;
            if (cv) {
                const float c = cv[i] * 0.2f;
                target *= c < 0.f ? 0.f : (c > 1.f ? 1.f : c);
            }
            const float g = gain_.process(target);
            o[i] = in ? in[i] * g : 0.f;
        }
        setMonitor(0, gain_.y);
    }

   private:
    Slew gain_;
};

Module* create() { return new Vca(); }

}  // namespace

const ModuleTypeInfo kVcaType = {
    "vca", "VCA", "Amplifier, CV x level", Category::Processor, Icon::Vca, Drawing::None,
    1, kParams, 2, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    15, sizeof(Vca) + kBlock * sizeof(float), 0, create};
