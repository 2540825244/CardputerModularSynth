#include "lfo.h"
#include "../dsp_util.h"
#include "common.h"

namespace {

enum { kWave, kRate };

// SIN and SQR keep the v1 values 0 and 1.
const char* const kWaveNames[] = {"SIN", "SQR", "TRI", "SAW", "RMP", "S&H", "SMO"};
const ParamSpec kParams[] = {
    {"wave", "WAV", 0.f, 6.f, 0.f, Curve::Stepped, Unit::Enum, kWaveNames},
    {"rate", "RAT", 0.01f, 40.f, 1.f, Curve::Log, Unit::Hz, nullptr},
};
const char* const kInKeys[] = {"rate", "rst"};
const char* const kInLabels[] = {"RAT", "RST"};
const char* const kOutKeys[] = {"out", "quad"};
const char* const kOutLabels[] = {"OUT", "90"};

// sin(2*pi*p), p in [0,1): parabola with one refinement step, error < 1e-3, no libm.
float fastSin(float p) {
    const float q = p < 0.5f ? p : p - 1.f;  // [-0.5, 0.5)
    const float y = 8.f * q * (1.f - 2.f * (q < 0.f ? -q : q));
    return 0.225f * (y * (y < 0.f ? -y : y) - y) + y;
}

class Lfo : public Module {
   public:
    Lfo() : Module(kLfoType) {}

    float wave(int w, float p, int seg) {
        switch (w) {
            case 1: return p < 0.5f ? 1.f : -1.f;
            case 2: return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
            case 3: return 2.f * p - 1.f;
            case 4: return 1.f - 2.f * p;
            case 5: return held_[seg];
            case 6: {  // smooth random: cosine between this cycle's and the next value
                const float t = 0.5f - 0.5f * fastSin(p * 0.5f + 0.25f);
                return held_[seg] + (held_[seg ^ 1] - held_[seg]) * t;
            }
            default: return fastSin(p);
        }
    }

    void process(const float* const* ins, const bool* connected) override {
        int w = (int)param(kWave);
        w = w < 0 ? 0 : (w > 6 ? 6 : w);
        const float base = param(kRate);
        const float* rc = connected[0] ? ins[0] : nullptr;
        const float* rst = connected[1] ? ins[1] : nullptr;
        float* o = out(0);
        float* q = out(1);
        for (int i = 0; i < kBlock; i++) {
            if (rst) {
                rstGate_.process(rst[i]);
                if (rstGate_.rose) phase_ = 0.f;
            }
            float hz = rc ? base * fastExp2(rc[i]) : base;
            hz = hz < 0.01f ? 0.01f : (hz > 40.f ? 40.f : hz);
            phase_ += hz * (1.f / kSampleRate);
            if (phase_ >= 1.f) {
                phase_ -= 1.f;
                seg_ ^= 1;  // a new random value for the next cycle
                held_[seg_ ^ 1] = rng_.bi();
            }
            float pq = phase_ + 0.25f;
            if (pq >= 1.f) pq -= 1.f;
            o[i] = wave(w, phase_, seg_) * kAudioVolts;
            q[i] = wave(w, pq, phase_ + 0.25f >= 1.f ? seg_ ^ 1 : seg_) * kAudioVolts;
        }
        setMonitor(0, phase_);
    }

   private:
    float phase_ = 0.f;
    float held_[2] = {0.f, 0.f};
    int seg_ = 0;
    mods::Rng rng_;
    GateDetector rstGate_;
};

Module* create() { return new Lfo(); }

}  // namespace

const ModuleTypeInfo kLfoType = {
    "lfo", "LFO", "LFO: 7 waves, reset, 90deg out", Category::Modulator, Icon::Lfo,
    Drawing::Picture, 2, kParams, 2, kInKeys, kInLabels, 2, kOutKeys, kOutLabels,
    420, sizeof(Lfo) + 2 * kBlock * sizeof(float), 0, create};
