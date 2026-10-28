#include "dly.h"
#include <cmath>
#include "../delay_pool.h"
#include "../dsp_util.h"

namespace {

enum { kTime, kFdbk, kMix };
enum { kInIn, kInTime };

constexpr float kStoreScale = 32767.f / 10.f;  // int16 holds volts / 10 (+-10 V)
constexpr float kLoadScale = 10.f / 32767.f;
constexpr float kSamplesPerMs = kSampleRate / 1000.f;

const ParamSpec kParams[] = {
    {"time", "TIM", 1.f, 1000.f, 250.f, Curve::Log, Unit::Ms, nullptr},
    {"fdbk", "FDB", 0.f, 0.95f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.35f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "time"};
const char* const kInLabels[] = {"IN", "TIM"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

class Dly : public Module {
   public:
    explicit Dly(int16_t* buf) : Module(kDlyType), buf_(buf) {
        timeSlew_.setTimeMs(50.f, kSampleRate);
        fbSlew_.setTimeMs(15.f, kSampleRate);
        mixSlew_.setTimeMs(15.f, kSampleRate);
    }
    ~Dly() override { delayPoolRelease(buf_); }

    void process(const float* const* ins, const bool* connected) override {
        const float ms = param(kTime);
        if (ms != lastMs_) {  // log2f only when the knob moved
            lastMs_ = ms;
            log2Samples_ = log2f(ms * kSamplesPerMs);
        }
        float fbTarget = param(kFdbk);
        fbTarget = fbTarget < 0.f ? 0.f : (fbTarget > 0.95f ? 0.95f : fbTarget);
        const float mixTarget = param(kMix);
        if (!seeded_) {  // first block: no slew from the default
            seeded_ = true;
            timeSlew_.y = log2Samples_;
            fbSlew_.y = fbTarget;
            mixSlew_.y = mixTarget;
        }
        const float* in = connected[kInIn] ? ins[kInIn] : nullptr;
        const float* tcv = connected[kInTime] ? ins[kInTime] : nullptr;
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float fb = fbSlew_.process(fbTarget);
            const float mix = mixSlew_.process(mixTarget);
            const float x = in ? sanitize(in[i]) : 0.f;
            float d = fastExp2(timeSlew_.process(log2Samples_) + (tcv ? tcv[i] : 0.f));
            d = d < 1.f ? 1.f : (d > (float)(kDelaySlotSamples - 2) ? (float)(kDelaySlotSamples - 2) : d);
            const int di = (int)d;
            const float frac = d - (float)di;
            int i0 = w_ - di;
            if (i0 < 0) i0 += kDelaySlotSamples;
            int i1 = i0 - 1;
            if (i1 < 0) i1 += kDelaySlotSamples;
            const float a = (float)buf_[i0], b = (float)buf_[i1];
            const float wet = (a + (b - a) * frac) * kLoadScale;
            float v = x + 10.f * softClip(fb * wet * 0.1f);
            v = v < -10.f ? -10.f : (v > 10.f ? 10.f : v);
            buf_[w_] = (int16_t)(v * kStoreScale);  // truncate: tails decay to 0
            if (++w_ >= kDelaySlotSamples) w_ = 0;
            o[i] = x * (1.f - mix) + wet * mix;
        }
    }

   private:
    int16_t* buf_;
    int w_ = 0;
    Slew timeSlew_;  // log2(delay in samples)
    Slew fbSlew_, mixSlew_;
    float lastMs_ = -1.f, log2Samples_ = 0.f;
    bool seeded_ = false;
};

Module* create() {
    int16_t* buf = delayPoolAcquire();
    return buf ? new Dly(buf) : nullptr;
}

}  // namespace

const ModuleTypeInfo kDlyType = {
    "dly", "DLY", "Delay to 1 s with feedback", Category::Effect, Icon::Dly,
    Drawing::None, 3, kParams, 2, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    30, sizeof(Dly) + kBlock * sizeof(float), kDelaySlots, create};
