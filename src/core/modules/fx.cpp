#include "fx.h"
#include <cmath>
#include "../delay_pool.h"
#include "Effects/flanger.h"
#include "Effects/tremolo.h"
#include "Synthesis/oscillator.h"
#include "common.h"

using namespace mods;

namespace {
const char* const kInOnly[] = {"in"};
const char* const kInLbl[] = {"IN"};
const char* const kOutOnly[] = {"out"};
const char* const kOutLbl[] = {"OUT"};

// sin(2*pi*p) for p in [0,1), parabolic, no libm.
float sine01(float p) {
    const float q = p < 0.5f ? p : p - 1.f;
    const float y = 8.f * q * (1.f - 2.f * (q < 0.f ? -q : q));
    return 0.225f * (y * (y < 0.f ? -y : y) - y) + y;
}
}  // namespace

// ---------------------------------------------------------------- REV
// Plate reverb after Dattorro (1997): pre-delay, 4 input diffusers, a figure-8 tank of two
// allpass + delay halves with damping. All lines live in one int16 pool slot.
namespace rev {
enum { kSize, kDecay, kDamp, kMix, kPre };
const ParamSpec kParams[] = {
    {"size", "SIZ", 0.3f, 1.f, 0.75f, Curve::Lin, Unit::Percent, nullptr},
    {"decay", "DEC", 0.f, 0.97f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"damp", "DMP", 0.f, 1.f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.35f, Curve::Lin, Unit::Percent, nullptr},
    {"pre", "PRE", 0.f, 200.f, 10.f, Curve::Sqr, Unit::Ms, nullptr},
};
const char* const kOutKeys[] = {"out", "wet"};
const char* const kOutLabels[] = {"OUT", "WET"};
// Line lengths at 48 kHz (Dattorro's 29.8 kHz figures x 1.61).
constexpr int kPreLen = 9600, kDiff[4] = {229, 173, 613, 448};
constexpr int kTank[2][4] = {{1086, 7195, 2909, 6010}, {1468, 6813, 4291, 5110}};

class Rev : public Module {
   public:
    explicit Rev(int16_t* buf) : Module(kRevType), slot_(buf) {
        int16_t* p = buf;
        auto take = [&p](Seg& s, int len) {
            s.buf = p;
            s.len = len;
            p += len;
        };
        take(pre_, kPreLen);
        for (int i = 0; i < 4; i++) take(diff_[i], kDiff[i]);
        for (int h = 0; h < 2; h++)
            for (int i = 0; i < 4; i++) take(tank_[h][i], kTank[h][i]);
        sizeSlew_.setTimeMs(200.f, kSampleRate);
        sizeSlew_.y = 0.75f;
    }
    ~Rev() override { delayPoolRelease(slot_); }

    static float allpass(Seg& s, float x, float g, int d) {
        const float z = s.read(d);
        const float v = x - g * z;
        s.write(v);
        return z + g * v;
    }

    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float decay = param(kDecay), damp = param(kDamp) * 0.7f, mix = param(kMix);
        const float preD = 1.f + param(kPre) * (kSampleRate / 1000.f);
        float* o = out(0);
        float* wo = out(1);
        for (int i = 0; i < kBlock; i++) {
            const float size = sizeSlew_.process(param(kSize));
            const float x = in ? sanitize(in[i]) : 0.f;
            pre_.write(x);
            float d = pre_.readFrac(clampf(preD, 1.f, (float)kPreLen - 2.f));
            bw_ += 0.7f * (d - bw_);
            d = bw_;
            d = allpass(diff_[0], d, 0.75f, kDiff[0]);
            d = allpass(diff_[1], d, 0.75f, kDiff[1]);
            d = allpass(diff_[2], d, 0.625f, kDiff[2]);
            d = allpass(diff_[3], d, 0.625f, kDiff[3]);
            float wet = 0.f;
            for (int h = 0; h < 2; h++) {
                Seg* t = tank_[h];
                float a = d + decay * fb_[1 - h];
                a = allpass(t[0], a, -0.7f, kTank[h][0]);
                const float dl1 = clampf((float)kTank[h][1] * size, 2.f, (float)kTank[h][1]);
                t[1].write(a);
                float b = t[1].readFrac(dl1);
                lp_[h] += (1.f - damp) * (b - lp_[h]);
                b = lp_[h] * decay;
                b = allpass(t[2], b, 0.5f, kTank[h][2]);
                const float dl2 = clampf((float)kTank[h][3] * size, 2.f, (float)kTank[h][3]);
                t[3].write(b);
                fb_[h] = softClip(t[3].readFrac(dl2) * 0.1f) * 10.f;
                wet += t[1].readFrac(dl1 * 0.45f) + t[3].readFrac(dl2 * 0.6f) - t[2].read(kTank[h][2] / 2);
            }
            wet *= 0.4f;
            wo[i] = wet;
            o[i] = x * (1.f - mix) + wet * mix;
        }
    }

   private:
    int16_t* slot_;
    Seg pre_, diff_[4], tank_[2][4];
    float bw_ = 0.f, lp_[2] = {}, fb_[2] = {};
    Slew sizeSlew_;
};

Module* create() {
    int16_t* buf = delayPoolAcquire();
    return buf ? new Rev(buf) : nullptr;
}
}  // namespace rev

const ModuleTypeInfo kRevType = {
    "rev", "REV", "Plate reverb", Category::Effect, Icon::Rev, Drawing::None,
    5, rev::kParams, 1, kInOnly, kInLbl, 2, rev::kOutKeys, rev::kOutLabels,
    1790, sizeof(rev::Rev) + 2 * kBlock * sizeof(float), kDelaySlots, rev::create, nullptr, 0, 1};

// ---------------------------------------------------------------- CHO
// Mono chorus: two taps of a 25 ms line swept in opposite phase.
namespace cho {
enum { kRate, kDepth, kMix, kFdbk };
const ParamSpec kParams[] = {
    {"rate", "RAT", 0.05f, 5.f, 0.6f, Curve::Log, Unit::Hz, nullptr},
    {"depth", "DEP", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"fdbk", "FDB", 0.f, 0.9f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
constexpr int kLen = 1200;
class Cho : public Module {
   public:
    Cho() : Module(kChoType) {}
    float tap(float d) const {
        float r = (float)w_ - d;
        while (r < 0.f) r += (float)kLen;
        const int i0 = (int)r, i1 = (i0 + 1) % kLen;
        const float f = r - (float)i0;
        return buf_[i0] + (buf_[i1] - buf_[i0]) * f;
    }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float inc = param(kRate) / kSampleRate, depth = param(kDepth) * 380.f, mix = param(kMix),
                    fb = param(kFdbk);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            ph_ += inc;
            if (ph_ >= 1.f) ph_ -= 1.f;
            const float m = sine01(ph_);
            const float a = tap(576.f + depth * m), b = tap(576.f - depth * m);
            const float x = in ? in[i] : 0.f;
            buf_[w_] = x + fb * a;
            w_ = (w_ + 1) % kLen;
            o[i] = x * (1.f - mix) + 0.5f * (a + b) * mix;
        }
    }

   private:
    float buf_[kLen] = {};
    int w_ = 0;
    float ph_ = 0.f;
};
Module* create() { return new Cho(); }
}  // namespace cho

const ModuleTypeInfo kChoType = {
    "cho", "CHO", "Chorus, two swept taps", Category::Effect, Icon::Fx, Drawing::None,
    4, cho::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    290, sizeof(cho::Cho) + kBlock * sizeof(float), 0, cho::create};

// ---------------------------------------------------------------- FLG
namespace flg {
enum { kRate, kDepth, kFdbk, kMix };
const ParamSpec kParams[] = {
    {"rate", "RAT", 0.05f, 5.f, 0.3f, Curve::Log, Unit::Hz, nullptr},
    {"depth", "DEP", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"fdbk", "FDB", 0.f, 0.95f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
class Flg : public Module {
   public:
    Flg() : Module(kFlgType) {
        f_.Init(kSampleRate);
        f_.SetDelayMs(1.f);
    }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        f_.SetLfoFreq(param(kRate));
        f_.SetLfoDepth(param(kDepth));
        f_.SetFeedback(param(kFdbk));
        const float mix = param(kMix);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float x = in ? in[i] * 0.2f : 0.f;
            const float y = f_.Process(x);
            o[i] = (x * (1.f - mix) + y * mix) * kAudioVolts;
        }
    }

   private:
    daisysp::Flanger f_;
};
Module* create() { return new Flg(); }
}  // namespace flg

const ModuleTypeInfo kFlgType = {
    "flg", "FLG", "Flanger with feedback", Category::Effect, Icon::Fx, Drawing::None,
    4, flg::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    210, sizeof(flg::Flg) + kBlock * sizeof(float), 0, flg::create};

// ---------------------------------------------------------------- PHA
// Phaser: 6 first-order allpasses, their corner swept by an LFO around FREQ.
namespace pha {
enum { kRate, kDepth, kFreq, kFdbk, kMix };
const ParamSpec kParams[] = {
    {"rate", "RAT", 0.05f, 8.f, 0.4f, Curve::Log, Unit::Hz, nullptr},
    {"depth", "DEP", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
    {"freq", "FRQ", 100.f, 4000.f, 700.f, Curve::Log, Unit::Hz, nullptr},
    {"fdbk", "FDB", 0.f, 0.9f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "rate"};
const char* const kInLabels[] = {"IN", "RAT"};
class Pha : public Module {
   public:
    Pha() : Module(kPhaType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* rc = inOrNull(ins, c, 1);
        const float depth = param(kDepth) * 2.f, fb = param(kFdbk), mix = param(kMix);
        const float center = log2f(param(kFreq) / kC4Hz);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            ph_ += param(kRate) * fastExp2(at(rc, i)) / kSampleRate;
            if (ph_ >= 1.f) ph_ -= 1.f;
            if ((i & 7) == 0) {  // coefficient every 8 samples
                const float hz = hzClamp(voltsToHz(center + depth * sine01(ph_)), 20.f, 16000.f);
                const float t = tanf(3.14159265f * hz / kSampleRate);
                a_ = (t - 1.f) / (t + 1.f);
            }
            const float x = in ? in[i] : 0.f;
            float y = x + fb * last_;
            for (float& z : z_) {  // y = a*x + z; z = x - a*y
                const float v = a_ * y + z;
                z = y - a_ * v;
                y = v;
            }
            last_ = softClip(y * 0.1f) * 10.f;
            o[i] = x * (1.f - mix) + y * mix;
        }
    }

   private:
    float z_[6] = {};
    float ph_ = 0.f, a_ = 0.f, last_ = 0.f;
};
Module* create() { return new Pha(); }
}  // namespace pha

const ModuleTypeInfo kPhaType = {
    "pha", "PHA", "6-stage phaser", Category::Effect, Icon::Fx, Drawing::None,
    5, pha::kParams, 2, pha::kInKeys, pha::kInLabels, 1, kOutOnly, kOutLbl,
    560, sizeof(pha::Pha) + kBlock * sizeof(float), 0, pha::create};

// ---------------------------------------------------------------- TRM
namespace trm {
enum { kRate, kDepth, kWave };
const char* const kWaves[] = {"SIN", "TRI", "SQR"};
const uint8_t kDaisy[] = {daisysp::Oscillator::WAVE_SIN, daisysp::Oscillator::WAVE_TRI,
                          daisysp::Oscillator::WAVE_SQUARE};
const ParamSpec kParams[] = {
    {"rate", "RAT", 0.1f, 20.f, 4.f, Curve::Log, Unit::Hz, nullptr},
    {"depth", "DEP", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"wave", "WAV", 0.f, 2.f, 0.f, Curve::Stepped, Unit::Enum, kWaves},
};
class Trm : public Module {
   public:
    Trm() : Module(kTrmType) { t_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        int w = (int)param(kWave);
        t_.SetWaveform(kDaisy[w < 0 ? 0 : (w > 2 ? 2 : w)]);
        t_.SetFreq(param(kRate));
        t_.SetDepth(param(kDepth));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) o[i] = t_.Process(in ? in[i] : 0.f);
    }

   private:
    daisysp::Tremolo t_;
};
Module* create() { return new Trm(); }
}  // namespace trm

const ModuleTypeInfo kTrmType = {
    "trm", "TRM", "Tremolo", Category::Effect, Icon::Fx, Drawing::None,
    3, trm::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    390, sizeof(trm::Trm) + kBlock * sizeof(float), 0, trm::create};

// ---------------------------------------------------------------- PSH
// Pitch shifter: two read heads sweep a 2048-sample window half a window apart, each faded by a
// triangle, so the reads overlap smoothly. Pitch in shifts by 1 V/oct on top of SEMI.
namespace psh {
enum { kSemi, kFine, kMix, kWin };
const ParamSpec kParams[] = {
    {"semi", "SEM", -24.f, 24.f, 12.f, Curve::Stepped, Unit::Semi, nullptr},
    {"fine", "FIN", -100.f, 100.f, 0.f, Curve::Lin, Unit::Cents, nullptr},
    {"mix", "MIX", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"win", "WIN", 10.f, 40.f, 30.f, Curve::Lin, Unit::Ms, nullptr},
};
const char* const kInKeys[] = {"in", "pitch"};
const char* const kInLabels[] = {"IN", "PIT"};
constexpr int kLen = 2048;
class Psh : public Module {
   public:
    Psh() : Module(kPshType) {}
    float tap(float d) const {
        float r = (float)w_ - d;
        while (r < 0.f) r += (float)kLen;
        const int i0 = (int)r % kLen, i1 = (i0 + 1) % kLen;
        const float f = r - (float)(int)r;
        return buf_[i0] + (buf_[i1] - buf_[i0]) * f;
    }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* pc = inOrNull(ins, c, 1);
        const float win = clampf(param(kWin) * (kSampleRate / 1000.f), 64.f, (float)kLen - 4.f);
        const float base = param(kSemi) / 12.f + param(kFine) / 1200.f, mix = param(kMix);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float ratio = fastExp2(base + at(pc, i));
            ph_ += (1.f - ratio) / win;
            ph_ -= floorf(ph_);
            const float p2 = ph_ + 0.5f >= 1.f ? ph_ - 0.5f : ph_ + 0.5f;
            const float g1 = 1.f - fabsf(2.f * ph_ - 1.f), g2 = 1.f - fabsf(2.f * p2 - 1.f);
            const float x = in ? in[i] : 0.f;
            buf_[w_] = x;
            const float y = tap(1.f + ph_ * win) * g1 + tap(1.f + p2 * win) * g2;
            w_ = (w_ + 1) % kLen;
            o[i] = x * (1.f - mix) + y * mix;
        }
    }

   private:
    float buf_[kLen] = {};
    int w_ = 0;
    float ph_ = 0.f;
};
Module* create() { return new Psh(); }
}  // namespace psh

const ModuleTypeInfo kPshType = {
    "psh", "PSH", "Pitch shifter, +-2 oct", Category::Effect, Icon::Psh, Drawing::None,
    4, psh::kParams, 2, psh::kInKeys, psh::kInLabels, 1, kOutOnly, kOutLbl,
    540, sizeof(psh::Psh) + kBlock * sizeof(float), 0, psh::create};
