#include "snh.h"
#include "../dsp_util.h"

namespace {

enum { kSlew };
enum { kInIn, kInTrig };

const ParamSpec kParams[] = {
    {"slew", "SLW", 0.f, 2000.f, 0.f, Curve::Sqr, Unit::Ms, nullptr},
};
const char* const kInKeys[] = {"in", "trig"};
const char* const kInLabels[] = {"IN", "TRG"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

// On each rising TRG, holds IN (or a random +-5 V value when IN is unpatched); the output glides
// to the held value over SLW.
class Snh : public Module {
   public:
    Snh() : Module(kSnhType) {}

    void process(const float* const* ins, const bool* connected) override {
        const float slew = param(kSlew);
        if (slew != lastSlew_) {
            lastSlew_ = slew;
            out_.setTimeMs(slew, kSampleRate);
        }
        const float* in = connected[kInIn] ? ins[kInIn] : nullptr;
        const float* trig = connected[kInTrig] ? ins[kInTrig] : nullptr;
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            if (trig) {
                gate_.process(trig[i]);
                if (gate_.rose) held_ = in ? in[i] : random();
            }
            o[i] = out_.process(held_);
        }
        setMonitor(0, held_);
    }

   private:
    float random() {  // xorshift32, uniform in [-5, 5)
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return (float)rng_ * (10.f / 4294967296.f) - 5.f;
    }

    GateDetector gate_;
    Slew out_;
    float held_ = 0.f, lastSlew_ = 0.f;
    uint32_t rng_ = 0x9E3779B9u;
};

Module* create() { return new Snh(); }

}  // namespace

const ModuleTypeInfo kSnhType = {
    "snh", "S&H", "Sample & hold; no IN = random", Category::Modulator, Icon::Snh, Drawing::None,
    1, kParams, 2, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    10, sizeof(Snh) + kBlock * sizeof(float), 0, create};
