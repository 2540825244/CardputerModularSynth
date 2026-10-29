// v1.3 Task 4: the restructured GRD (passes, block ladder) must give the old GRD's output
// sample for sample. OldGrd and OldLadder are the v1.3-Task-3 code, verbatim apart from names.
#define TEST  // stmlib's portable code paths, as grd.cpp
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>
#include "Control/adsr.h"
#include "core/dsp_util.h"
#include "core/module.h"
#include "core/modules/common.h"
#include "core/modules/grd.h"
#include "plaits/dsp/voice.h"

using namespace mods;

namespace oldgrd {
enum { kEng, kOct, kTune, kHarm, kTimb, kMorph, kFmAmt, kTAmt, kMAmt, kDecay, kColor,
       kVca, kFlt, kCut, kRes, kFeg, kAtk, kDcy, kSus, kLRate, kLCut, kLTim };
enum { kInPitch, kInTrig, kInFm, kInHarm, kInTimb, kInMorph, kInLevel, kInEng, kInCut };
enum { kVcaEnv, kVcaLpg, kVcaDrone };
enum { kFltLp, kFltHp, kFltOff };
constexpr size_t kShared = 16384;
constexpr int kChunk = (int)plaits::kBlockSize;

// DaisySP's LadderFilter (Huovilainen new Moog model, van Hoesel v1.7: 4x linearly interpolated
// oversampling, six modes) with the same coefficients and response, rewritten for the ESP32-S3:
// the per-step tanh division is a recip0.s reciprocal, the stages are inline with the state in
// locals, and the mode mix is chosen once per sample instead of in every step. DaisySP's version
// measured ~960 cycles/sample on the device.
class OldLadder {
   public:
    enum Mode { LP24, LP12, BP24, BP12, HP24, HP12 };  // DaisySP's FilterMode order

    void Init(float sampleRate) {
        sr_ = sampleRate;
        srIntRecip_ = 1.f / (sampleRate * kOs);
        for (int i = 0; i < 4; i++) z0_[i] = z1_[i] = 0.f;
        old_ = 0.f;
        mode_ = LP24;
        SetPassbandGain(0.5f);
        SetInputDrive(0.5f);
        SetFreq(5000.f);
        SetRes(0.2f);
    }
    void SetFreq(float hz) {
        hz = hz < 5.f ? 5.f : (hz > sr_ * 0.425f ? sr_ * 0.425f : hz);
        const float wc = hz * 2.f * 3.14159265f * srIntRecip_, wc2 = wc * wc;
        alpha_ = 0.9892f * wc - 0.4324f * wc2 + 0.1381f * wc * wc2 - 0.0202f * wc2 * wc2;
        qAdjust_ = 1.006f + 0.0536f * wc - 0.095f * wc2 - 0.05f * wc2 * wc2;
    }
    void SetRes(float res) { k_ = 4.f * (res < 0.f ? 0.f : (res > 1.8f ? 1.8f : res)); }
    void SetPassbandGain(float pbg) {
        pbg_ = pbg < 0.f ? 0.f : (pbg > 0.5f ? 0.5f : pbg);
        SetInputDrive(drive_);
    }
    void SetInputDrive(float d) {
        drive_ = d > 0.f ? d : 0.f;
        if (drive_ > 1.f) {
            drive_ = drive_ < 4.f ? drive_ : 4.f;
            driveScaled_ = 1.f + (drive_ - 1.f) * (1.f - pbg_);
        } else {
            driveScaled_ = drive_;
        }
    }
    void SetFilterMode(Mode m) { mode_ = m; }

    float Process(float in) {
        switch (mode_) {
            case LP24: return run<LP24>(in);
            case LP12: return run<LP12>(in);
            case BP24: return run<BP24>(in);
            case BP12: return run<BP12>(in);
            case HP24: return run<HP24>(in);
            default: return run<HP12>(in);
        }
    }

   private:
    static constexpr int kOs = 4;

    static float tanhApprox(float x) {
        if (x > 3.f) return 1.f;
        if (x < -3.f) return -1.f;
        const float x2 = x * x;
        return x * (27.f + x2) * recipPos(27.f + 9.f * x2);
    }

    template <int M>
    static float mix(float u, float s1, float s2, float s3, float s4) {
        switch (M) {
            case LP24: return s4;
            case LP12: return s2;
            case BP24: return (s2 + s4) * 4.f - s3 * 8.f;
            case BP12: return (s1 - s2) * 2.f;
            case HP24: return u + s4 - (s1 + s3) * 4.f + s2 * 6.f;
            default: return u + s2 - s1 * 2.f;
        }
    }

    template <int M>
    float run(float in) {
        const float input = in * driveScaled_;
        const float kq = k_ * qAdjust_, a = alpha_, pbg = pbg_;
        float a0 = z0_[0], a1 = z0_[1], a2 = z0_[2], a3 = z0_[3];
        float b0 = z1_[0], b1 = z1_[1], b2 = z1_[2], b3 = z1_[3];
        float total = 0.f, interp = 0.f;
        for (int os = 0; os < kOs; os++) {
            const float x = interp * old_ + (1.f - interp) * input;
            const float u = tanhApprox(x - (b3 - pbg * x) * kq);
            // stage: ft = s * (1 / 1.3) + (0.3 / 1.3) * z0 - z1; ft = ft * alpha + z1
            float f = (u * 0.76923077f + 0.23076923f * a0 - b0) * a + b0;
            a0 = u;
            b0 = f;
            const float s1 = f;
            f = (s1 * 0.76923077f + 0.23076923f * a1 - b1) * a + b1;
            a1 = s1;
            b1 = f;
            const float s2 = f;
            f = (s2 * 0.76923077f + 0.23076923f * a2 - b2) * a + b2;
            a2 = s2;
            b2 = f;
            const float s3 = f;
            f = (s3 * 0.76923077f + 0.23076923f * a3 - b3) * a + b3;
            a3 = s3;
            b3 = f;
            total += mix<M>(u, s1, s2, s3, f) * (1.f / kOs);
            interp += 1.f / kOs;
        }
        z0_[0] = a0, z0_[1] = a1, z0_[2] = a2, z0_[3] = a3;
        z1_[0] = b0, z1_[1] = b1, z1_[2] = b2, z1_[3] = b3;
        old_ = input;
        return total;
    }

    float sr_ = 48000.f, srIntRecip_ = 0.f, alpha_ = 1.f, k_ = 1.f, qAdjust_ = 1.f, pbg_ = 0.5f;
    float drive_ = 0.5f, driveScaled_ = 0.5f, old_ = 0.f;
    float z0_[4] = {}, z1_[4] = {};
    Mode mode_ = LP24;
};

class OldGrd : public Module {
   public:
    explicit OldGrd(char* shared) : Module(kGrdType), shared_(shared) {
        stmlib::BufferAllocator a(shared_, kShared);
        voice_.Init(&a);
        lad_.Init(kSampleRate);
        env_.Init(kSampleRate);
        cutSlew_.setTimeMs(10.f, kSampleRate / 8.f);
    }
    ~OldGrd() override { delete[] shared_; }

    void process(const float* const* ins, const bool* c) override {
        const int vca = (int)param(kVca), flt = (int)param(kFlt);
        plaits::Patch p = {};
        p.note = 60.f + param(kOct) * 12.f + param(kTune);
        p.harmonics = param(kHarm);
        p.morph = param(kMorph);
        // A patched CV is scaled by its input's attenuverter, so Plaits gets amount 1 (its
        // curve makes 1 -> ~1). The amount params are the internal envelope's (TRG patched).
        p.frequency_modulation_amount = c[kInFm] ? 1.f : param(kFmAmt);
        p.timbre_modulation_amount = c[kInTimb] ? 1.f : param(kTAmt);
        p.morph_modulation_amount = c[kInMorph] ? 1.f : param(kMAmt);
        p.engine = grdEngineIndex((int)param(kEng));
        p.decay = param(kDecay);
        p.lpg_colour = param(kColor);
        const float timb = param(kTimb), lTim = param(kLTim) * 0.5f, lCut = param(kLCut) * 3.f;
        const float feg = param(kFeg) * 6.f;
        env_.SetAttackTime(param(kAtk) * 0.001f);
        env_.SetDecayTime(param(kDcy) * 0.001f);
        env_.SetReleaseTime(param(kDcy) * 0.001f);
        env_.SetSustainLevel(param(kSus));
        lad_.SetFilterMode(flt == kFltHp ? OldLadder::HP24 : OldLadder::LP24);
        lad_.SetRes(param(kRes) * 1.8f);
        const float base = log2f(param(kCut) / kC4Hz);
        if (!seeded_) {
            seeded_ = true;
            cutSlew_.y = base;
        }
        const float lfoInc = param(kLRate) * (1.f / kSampleRate);
        const bool gated = c[kInTrig];
        float* o = out(0);
        float* x = out(1);
        float* eo = out(2);
        float* lo = out(3);
        float hz = 0.f;
        for (int i = 0; i < kBlock; i++) {
            lfoPh_ += lfoInc;
            if (lfoPh_ >= 1.f) lfoPh_ -= 1.f;
            const float lfo = lfoPh_ < 0.5f ? 4.f * lfoPh_ - 1.f : 3.f - 4.f * lfoPh_;
            // Triggers seen since the last render go to the next one (<= 12 samples late; Plaits
            // delays them 1 ms anyway), so no pulse falls between chunks or blocks.
            if (gated && ins[kInTrig][i] > trigMax_) trigMax_ = ins[kInTrig][i];
            if (pos_ == kChunk) {  // render the next 12 samples with the CVs as they are now
                p.timbre = clampf(timb + lfo * lTim, 0.f, 1.f);
                render(p, ins, c, i, vca);
                trigMax_ = 0.f;
                pos_ = 0;
            }
            const float e = env_.Process(gate_.process(gated ? ins[kInTrig][i] : 0.f));
            if ((i & 7) == 0) {
                const float oct = cutSlew_.process(base) + feg * e + lCut * lfo + at(inOrNull(ins, c, kInCut), i);
                hz = hzClamp(voltsToHz(oct), 20.f, 18000.f);
                lad_.SetFreq(hz);
            }
            const plaits::Voice::Frame& f = fifo_[pos_++];
            float v = (float)f.out * (1.f / 32768.f);
            if (flt != kFltOff) v = lad_.Process(v);
            float g = vca == kVcaEnv && gated ? e : 1.f;
            if (vca != kVcaLpg && c[kInLevel]) g *= clampf(ins[kInLevel][i] * 0.2f, 0.f, 1.f);
            o[i] = v * g * kAudioVolts;
            x[i] = (float)f.aux * (kAudioVolts / 32768.f);
            eo[i] = e * kAudioVolts;
            lo[i] = lfo * kAudioVolts;
        }
        setMonitor(0, (float)voice_.active_engine());
        setMonitor(1, hz);
    }

   private:
    void render(const plaits::Patch& p, const float* const* ins, const bool* c, int s, int vca) {
        plaits::Modulations m = {};
        // CVs: pitch 1 V/oct; 5 V = the full knob range elsewhere (attenuate with the input amounts).
        auto cv = [&](int in) { return c[in] ? ins[in][s] * 0.2f : 0.f; };
        m.note = c[kInPitch] ? ins[kInPitch][s] * 12.f : 0.f;
        m.frequency = c[kInFm] ? ins[kInFm][s] * 12.f : 0.f;
        m.harmonics = cv(kInHarm);
        m.timbre = cv(kInTimb);
        m.morph = cv(kInMorph);
        m.engine = cv(kInEng);
        m.frequency_patched = c[kInFm];
        m.timbre_patched = c[kInTimb];
        m.morph_patched = c[kInMorph];
        m.trigger_patched = c[kInTrig];
        // Level < 1: at an accent of exactly 1 the 6-op engine reads past lut_cube_root.
        if (vca == kVcaLpg) {
            m.level = clampf(cv(kInLevel), -1.f, 0.99f);
            m.level_patched = c[kInLevel];
        } else {  // our VCA: hold Plaits' LPG open; triggers still excite the engines
            m.level = 0.99f;
            m.level_patched = true;
        }
        m.trigger = trigMax_ * 0.2f;
        voice_.Render(p, m, fifo_, kChunk);
    }

    char* shared_;
    plaits::Voice voice_;
    plaits::Voice::Frame fifo_[kChunk];
    int pos_ = kChunk;  // next frame to play; kChunk = render first
    float trigMax_ = 0.f;
    OldLadder lad_;
    daisysp::Adsr env_;
    GateDetector gate_;
    Slew cutSlew_;
    float lfoPh_ = 0.f;
    bool seeded_ = false;
};

}  // namespace oldgrd
