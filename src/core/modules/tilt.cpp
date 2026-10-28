#include "tilt.h"
#include <atomic>
#include "../dsp_util.h"

namespace {

std::atomic<float> g_roll{0.f}, g_pitch{0.f};

enum { kRange, kSmooth };

const ParamSpec kParams[] = {
    {"range", "RNG", 10.f, 90.f, 45.f, Curve::Lin, Unit::Deg, nullptr},
    {"smooth", "SMO", 5.f, 500.f, 20.f, Curve::Sqr, Unit::Ms, nullptr},  // >= 5 ms: input updates at ~100 Hz
};
const char* const kOutKeys[] = {"roll", "pitch"};
const char* const kOutLabels[] = {"ROL", "PIT"};

// Device tilt as CV: +-RNG degrees maps to +-5 V, clamped, smoothed over SMO.
class Tilt : public Module {
   public:
    Tilt() : Module(kTiltType) {}

    void process(const float* const*, const bool*) override {
        const float range = param(kRange), smooth = param(kSmooth);
        if (smooth != lastSmooth_) {
            lastSmooth_ = smooth;
            roll_.setTimeMs(smooth, kSampleRate);
            pitch_.setTimeMs(smooth, kSampleRate);
        }
        const float k = kAudioVolts / (range > 1.f ? range : 1.f);
        const float r = clamp5(g_roll.load(std::memory_order_relaxed) * k);
        const float p = clamp5(g_pitch.load(std::memory_order_relaxed) * k);
        float* o0 = out(0);
        float* o1 = out(1);
        for (int i = 0; i < kBlock; i++) {
            o0[i] = roll_.process(r);
            o1[i] = pitch_.process(p);
        }
        setMonitor(0, roll_.y);
        setMonitor(1, pitch_.y);
    }

   private:
    static float clamp5(float v) { return v > kAudioVolts ? kAudioVolts : (v < -kAudioVolts ? -kAudioVolts : v); }
    Slew roll_, pitch_;
    float lastSmooth_ = -1.f;
};

Module* create() { return new Tilt(); }

}  // namespace

void tiltPublish(float rollDeg, float pitchDeg) {
    g_roll.store(rollDeg, std::memory_order_relaxed);
    g_pitch.store(pitchDeg, std::memory_order_relaxed);
}

const ModuleTypeInfo kTiltType = {
    "tilt", "TLT", "Device tilt as CV", Category::Modulator, Icon::Tilt, Drawing::None,
    2, kParams, 0, nullptr, nullptr, 2, kOutKeys, kOutLabels,
    25, sizeof(Tilt) + 2 * kBlock * sizeof(float), 0, create};
