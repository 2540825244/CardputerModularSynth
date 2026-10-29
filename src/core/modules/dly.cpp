#include "dly.h"
#include <cmath>
#include "../delay_pool.h"
#include "../dsp_util.h"

namespace {

enum { kTime, kFdbk, kMix, kTone, kSync };
enum { kInIn, kInTime, kInClk };

constexpr float kStoreScale = 32767.f / 10.f;  // int16 holds volts / 10 (+-10 V)
constexpr float kLoadScale = 10.f / 32767.f;
constexpr float kSamplesPerMs = kSampleRate / 1000.f;

// With CLK patched, the delay is the clock period times SYNC (TIME is ignored).
const char* const kSyncNames[] = {"1/4", "1/3", "1/2", "2/3", "3/4", "1/1"};
const float kSyncMul[] = {0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 0.75f, 1.f};
const ParamSpec kParams[] = {
    {"time", "TIM", 1.f, 1000.f, 250.f, Curve::Log, Unit::Ms, nullptr},
    {"fdbk", "FDB", 0.f, 0.95f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.35f, Curve::Lin, Unit::Percent, nullptr},
    {"tone", "TON", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"sync", "SYN", 0.f, 5.f, 3.f, Curve::Stepped, Unit::Enum, kSyncNames},
};
const char* const kInKeys[] = {"in", "time", "clk"};
const char* const kInLabels[] = {"IN", "TIM", "CLK"};
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
        const float* clk = connected[kInClk] ? ins[kInClk] : nullptr;
        int si = (int)param(kSync);
        si = si < 0 ? 0 : (si > 5 ? 5 : si);
        const float toneK = 0.05f + 0.95f * param(kTone);  // one-pole in the feedback path
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            if (clk) {  // clock period, in samples, from rising edge to rising edge
                sinceEdge_++;
                if (clkGate_.process(clk[i]) && clkGate_.rose) {
                    if (sinceEdge_ > 8 && sinceEdge_ < kDelaySlotSamples) clockLog2_ = log2f((float)sinceEdge_ * kSyncMul[si]);
                    sinceEdge_ = 0;
                }
            }
            const float fb = fbSlew_.process(fbTarget);
            const float mix = mixSlew_.process(mixTarget);
            const float x = in ? sanitize(in[i]) : 0.f;
            const float target = clk && clockLog2_ > 0.f ? clockLog2_ : log2Samples_;
            float d = fastExp2(timeSlew_.process(target) + (tcv ? tcv[i] : 0.f));
            d = d < 1.f ? 1.f : (d > (float)(kDelaySlotSamples - 2) ? (float)(kDelaySlotSamples - 2) : d);
            const int di = (int)d;
            const float frac = d - (float)di;
            int i0 = w_ - di;
            if (i0 < 0) i0 += kDelaySlotSamples;
            int i1 = i0 - 1;
            if (i1 < 0) i1 += kDelaySlotSamples;
            const float a = (float)buf_[i0], b = (float)buf_[i1];
            const float wet = (a + (b - a) * frac) * kLoadScale;
            toneY_ += toneK * (wet - toneY_);
            float v = x + 10.f * softClip(fb * toneY_ * 0.1f);
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
    float lastMs_ = -1.f, log2Samples_ = 0.f, clockLog2_ = 0.f, toneY_ = 0.f;
    GateDetector clkGate_;
    int sinceEdge_ = 0;
    bool seeded_ = false;
};

Module* create() {
    int16_t* buf = delayPoolAcquire();
    return buf ? new Dly(buf) : nullptr;
}

}  // namespace

const ModuleTypeInfo kDlyType = {
    "dly", "DLY", "Delay 1 s, tone, clock sync", Category::Effect, Icon::Dly,
    Drawing::Picture, 5, kParams, 3, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    510, sizeof(Dly) + kBlock * sizeof(float), kDelaySlots, create, nullptr, 0, 1};
