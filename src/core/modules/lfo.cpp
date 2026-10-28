#include "lfo.h"
#include "../dsp_util.h"

namespace {

enum { kWave, kRate };

const char* const kWaveNames[] = {"SIN", "SQR"};
const ParamSpec kParams[] = {
    {"wave", "WAV", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kWaveNames},
    {"rate", "RAT", 0.01f, 40.f, 1.f, Curve::Log, Unit::Hz, nullptr},
};
const char* const kInKeys[] = {"rate"};
const char* const kInLabels[] = {"RAT"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

// sin(2*pi*p), p in [0,1): parabola with one refinement step, error < 1e-3, no libm.
float fastSin(float p) {
    const float q = p < 0.5f ? p : p - 1.f;  // [-0.5, 0.5)
    const float y = 8.f * q * (1.f - 2.f * (q < 0.f ? -q : q));
    return 0.225f * (y * (y < 0.f ? -y : y) - y) + y;
}

class Lfo : public Module {
   public:
    Lfo() : Module(kLfoType) {}

    void process(const float* const* ins, const bool* connected) override {
        const bool sqr = param(kWave) >= 0.5f;
        const float base = param(kRate);
        const float* rc = connected[0] ? ins[0] : nullptr;
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            float hz = rc ? base * fastExp2(rc[i]) : base;
            hz = hz < 0.01f ? 0.01f : (hz > 40.f ? 40.f : hz);
            phase_ += hz * (1.f / kSampleRate);
            if (phase_ >= 1.f) phase_ -= 1.f;
            o[i] = (sqr ? (phase_ < 0.5f ? 1.f : -1.f) : fastSin(phase_)) * kAudioVolts;
        }
        setMonitor(0, phase_);
    }

   private:
    float phase_ = 0.f;
};

Module* create() { return new Lfo(); }

}  // namespace

const ModuleTypeInfo kLfoType = {
    "lfo", "LFO", "Sine/square LFO, rate CV", Category::Modulator, Icon::Lfo,
    Drawing::None, 2, kParams, 1, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    12, sizeof(Lfo) + kBlock * sizeof(float), 0, create};
