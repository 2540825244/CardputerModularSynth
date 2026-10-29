#include "proc.h"
#include <cmath>
#include "Effects/autowah.h"
#include "Effects/overdrive.h"
#include "Effects/wavefolder.h"
#include "../ladder.h"
#include "common.h"

using namespace mods;

namespace {
const char* const kInOnly[] = {"in"};
const char* const kInLbl[] = {"IN"};
const char* const kOutOnly[] = {"out"};
const char* const kOutLbl[] = {"OUT"};
}  // namespace

// ---------------------------------------------------------------- LAD
namespace lad {
enum { kCut, kRes, kDrive, kMode };
const char* const kModes[] = {"LP24", "LP12", "BP24", "BP12", "HP24", "HP12"};
const ParamSpec kParams[] = {
    {"cutoff", "CUT", 20.f, 18000.f, 800.f, Curve::Log, Unit::Hz, nullptr},
    {"res", "RES", 0.f, 1.f, 0.3f, Curve::Lin, Unit::Percent, nullptr},
    {"drive", "DRV", 0.f, 1.f, 0.2f, Curve::Lin, Unit::Percent, nullptr},
    {"mode", "MOD", 0.f, 5.f, 0.f, Curve::Stepped, Unit::Enum, kModes},
};
const char* const kInKeys[] = {"in", "cut", "res"};
const char* const kInLabels[] = {"IN", "CUT", "RES"};
class Lad : public Module {
   public:
    Lad() : Module(kLadType) {
        f_.Init(kSampleRate);
        slew_.setTimeMs(10.f, kSampleRate / 8.f);
    }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* cut = inOrNull(ins, c, 1);
        const float* rcv = inOrNull(ins, c, 2);
        int md = (int)param(kMode);
        f_.SetFilterMode((Ladder::Mode)(md < 0 ? 0 : (md > 5 ? 5 : md)));
        f_.SetInputDrive(1.f + 3.f * param(kDrive));
        const float base = log2f(param(kCut) / kC4Hz);
        if (!seeded_) {
            seeded_ = true;
            slew_.y = base;
        }
        float* o = out(0);
        float hz = 0.f;
        for (int i = 0; i < kBlock; i += 8) {
            hz = hzClamp(voltsToHz(slew_.process(base) + at(cut, i)), 20.f, 18000.f);
            f_.SetFreq(hz);
            f_.SetRes(clampf(param(kRes) + at(rcv, i) * 0.2f, 0.f, 1.f) * 1.8f);
            for (int j = i; j < i + 8; j++) o[j] = in ? in[j] * 0.2f : 0.f;
            f_.ProcessBlock(o + i, 8);  // the state stays in registers over the 8 samples
            for (int j = i; j < i + 8; j++) o[j] *= kAudioVolts;
        }
        setMonitor(0, hz);
    }

   private:
    Ladder f_;
    Slew slew_;
    bool seeded_ = false;
};
Module* create() { return new Lad(); }
}  // namespace lad

const ModuleTypeInfo kLadType = {
    "lad", "LAD", "Ladder filter 24/12 dB, 6 modes", Category::Processor, Icon::Vcf, Drawing::None,
    4, lad::kParams, 3, lad::kInKeys, lad::kInLabels, 1, kOutOnly, kOutLbl,
    790, sizeof(lad::Lad) + kBlock * sizeof(float), 0, lad::create};

// ---------------------------------------------------------------- FLD
namespace fld {
enum { kGain, kOfs };
const ParamSpec kParams[] = {
    {"gain", "GAN", 1.f, 10.f, 2.f, Curve::Log, Unit::Ratio, nullptr},
    {"ofs", "OFS", -1.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "gain"};
const char* const kInLabels[] = {"IN", "GAN"};
class Fld : public Module {
   public:
    Fld() : Module(kFldType) { f_.Init(); }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* g = inOrNull(ins, c, 1);
        f_.SetOffset(param(kOfs));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            f_.SetGain(clampf(param(kGain) * fastExp2(at(g, i) * 0.4f), 0.5f, 40.f));
            o[i] = clampf(f_.Process(in ? in[i] * 0.2f : 0.f), -1.f, 1.f) * kAudioVolts;
        }
    }

   private:
    daisysp::Wavefolder f_;
};
Module* create() { return new Fld(); }
}  // namespace fld

const ModuleTypeInfo kFldType = {
    "fld", "FLD", "Wavefolder, gain CV", Category::Processor, Icon::Fold, Drawing::None,
    2, fld::kParams, 2, fld::kInKeys, fld::kInLabels, 1, kOutOnly, kOutLbl,
    400, sizeof(fld::Fld) + kBlock * sizeof(float), 0, fld::create};

// ---------------------------------------------------------------- OVD
namespace ovd {
enum { kDrive, kTone, kMix };
const ParamSpec kParams[] = {
    {"drive", "DRV", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"tone", "TON", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "drive"};
const char* const kInLabels[] = {"IN", "DRV"};
class Ovd : public Module {
   public:
    Ovd() : Module(kOvdType) { d_.Init(); }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* dc = inOrNull(ins, c, 1);
        const float k = 0.02f + 0.98f * param(kTone) * param(kTone), mix = param(kMix);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            if ((i & 7) == 0) d_.SetDrive(clampf(param(kDrive) + at(dc, i) * 0.1f, 0.f, 1.f));
            const float x = in ? in[i] * 0.2f : 0.f;
            lp_ += k * (d_.Process(x) - lp_);
            o[i] = (x * (1.f - mix) + lp_ * mix) * kAudioVolts;
        }
    }

   private:
    daisysp::Overdrive d_;
    float lp_ = 0.f;
};
Module* create() { return new Ovd(); }
}  // namespace ovd

const ModuleTypeInfo kOvdType = {
    "ovd", "OVD", "Overdrive with tone", Category::Processor, Icon::Drive, Drawing::None,
    3, ovd::kParams, 2, ovd::kInKeys, ovd::kInLabels, 1, kOutOnly, kOutLbl,
    250, sizeof(ovd::Ovd) + kBlock * sizeof(float), 0, ovd::create};

// ---------------------------------------------------------------- CRU
// Bit crusher + sample-rate reducer: hold every Nth sample, quantise to BITS.
namespace cru {
enum { kBits, kRate, kMix };
const ParamSpec kParams[] = {
    {"bits", "BIT", 1.f, 16.f, 8.f, Curve::Stepped, Unit::Int, nullptr},
    {"rate", "RAT", 200.f, 48000.f, 12000.f, Curve::Log, Unit::Hz, nullptr},
    {"mix", "MIX", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "rate"};
const char* const kInLabels[] = {"IN", "RAT"};
class Cru : public Module {
   public:
    Cru() : Module(kCruType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* rc = inOrNull(ins, c, 1);
        const float steps = (float)(1 << ((int)param(kBits) - 1));
        const float mix = param(kMix);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float x = in ? in[i] : 0.f;
            ph_ += clampf(param(kRate) * fastExp2(at(rc, i)), 50.f, kSampleRate) / kSampleRate;
            if (ph_ >= 1.f) {
                ph_ -= floorf(ph_);
                held_ = roundf(clampf(x * 0.2f, -1.f, 1.f) * steps) / steps * kAudioVolts;
            }
            o[i] = x * (1.f - mix) + held_ * mix;
        }
    }

   private:
    float ph_ = 1.f, held_ = 0.f;
};
Module* create() { return new Cru(); }
}  // namespace cru

const ModuleTypeInfo kCruType = {
    "cru", "CRU", "Bit crusher + rate reducer", Category::Processor, Icon::Crush, Drawing::None,
    3, cru::kParams, 2, cru::kInKeys, cru::kInLabels, 1, kOutOnly, kOutLbl,
    460, sizeof(cru::Cru) + kBlock * sizeof(float), 0, cru::create};

// ---------------------------------------------------------------- LPG
// Low-pass gate: a vactrol-like envelope (fast rise, DECAY fall) opens a VCA and, with COLOR, a
// 2-pole lowpass. CV opens it continuously; a TRIG strikes it fully open.
namespace lpg {
enum { kDecay, kColor, kLevel };
const ParamSpec kParams[] = {
    {"decay", "DEC", 10.f, 3000.f, 300.f, Curve::Log, Unit::Ms, nullptr},
    {"color", "COL", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
    {"level", "LVL", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "cv", "trig"};
const char* const kInLabels[] = {"IN", "CV", "TRG"};
class Lpg : public Module {
   public:
    Lpg() : Module(kLpgType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* cv = inOrNull(ins, c, 1);
        const float* tr = inOrNull(ins, c, 2);
        const float fall = 1.f - expf(-1.f / (param(kDecay) * 0.001f * kSampleRate));
        const float rise = 1.f - expf(-1.f / (0.002f * kSampleRate));
        const float color = param(kColor), level = param(kLevel);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            if (tr) {
                gate_.process(tr[i]);
                if (gate_.rose) env_ = 1.f;
            }
            const float target = clampf(level + at(cv, i) * 0.2f, 0.f, 1.f);
            env_ += (target > env_ ? rise : fall) * (target - env_);
            if ((i & 7) == 0) {
                const float hz = 40.f * powf(400.f, env_ * color + (1.f - color));
                k_ = 1.f - expf(-6.2831853f * hzClamp(hz, 20.f, 18000.f) / kSampleRate);
            }
            const float x = (in ? in[i] : 0.f) * env_;
            s1_ += k_ * (x - s1_);
            s2_ += k_ * (s1_ - s2_);
            o[i] = s2_;
        }
        setMonitor(0, env_);
    }

   private:
    GateDetector gate_;
    float env_ = 0.f, k_ = 1.f, s1_ = 0.f, s2_ = 0.f;
};
Module* create() { return new Lpg(); }
}  // namespace lpg

const ModuleTypeInfo kLpgType = {
    "lpg", "LPG", "Low-pass gate, vactrol decay", Category::Processor, Icon::Lpg, Drawing::Picture,
    3, lpg::kParams, 3, lpg::kInKeys, lpg::kInLabels, 1, kOutOnly, kOutLbl,
    320, sizeof(lpg::Lpg) + kBlock * sizeof(float), 0, lpg::create};

// ---------------------------------------------------------------- RNG
// Ring modulator and math: A x B / 5 V, A + B, max, min.
namespace ring {
const char* const kInKeys[] = {"a", "b"};
const char* const kInLabels[] = {"A", "B"};
const char* const kOutKeys[] = {"mul", "sum", "max", "min"};
const char* const kOutLabels[] = {"MUL", "SUM", "MAX", "MIN"};
class Ring : public Module {
   public:
    Ring() : Module(kRingType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* a = inOrNull(ins, c, 0);
        const float* b = inOrNull(ins, c, 1);
        for (int i = 0; i < kBlock; i++) {
            const float x = at(a, i), y = b ? b[i] : (a ? 5.f : 0.f);  // B unpatched: x 1
            out(0)[i] = x * y * (1.f / kAudioVolts);
            out(1)[i] = clampf(x + at(b, i), -12.f, 12.f);
            out(2)[i] = x > at(b, i) ? x : at(b, i);
            out(3)[i] = x < at(b, i) ? x : at(b, i);
        }
    }
};
Module* create() { return new Ring(); }
}  // namespace ring

const ModuleTypeInfo kRingType = {
    "ring", "RNG", "Ring mod, sum, max, min", Category::Processor, Icon::Ring, Drawing::None,
    0, nullptr, 2, ring::kInKeys, ring::kInLabels, 4, ring::kOutKeys, ring::kOutLabels,
    260, sizeof(ring::Ring) + 4 * kBlock * sizeof(float), 0, ring::create};

// ---------------------------------------------------------------- XFD
namespace xfd {
enum { kPos };
const ParamSpec kParams[] = {
    {"pos", "POS", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"a", "b", "pos"};
const char* const kInLabels[] = {"A", "B", "POS"};
class Xfd : public Module {
   public:
    Xfd() : Module(kXfdType) { s_.setTimeMs(2.f, kSampleRate); s_.y = 0.5f; }
    void process(const float* const* ins, const bool* c) override {
        const float* a = inOrNull(ins, c, 0);
        const float* b = inOrNull(ins, c, 1);
        const float* p = inOrNull(ins, c, 2);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float x = clampf(s_.process(param(kPos)) + at(p, i) * 0.2f, 0.f, 1.f);
            o[i] = at(a, i) * (1.f - x) + at(b, i) * x;
        }
    }

   private:
    Slew s_;
};
Module* create() { return new Xfd(); }
}  // namespace xfd

const ModuleTypeInfo kXfdType = {
    "xfd", "XFD", "Crossfade A to B, CV", Category::Utility, Icon::Mix, Drawing::None,
    1, xfd::kParams, 3, xfd::kInKeys, xfd::kInLabels, 1, kOutOnly, kOutLbl,
    220, sizeof(xfd::Xfd) + kBlock * sizeof(float), 0, xfd::create};

// ---------------------------------------------------------------- DYN
// Compressor/limiter with an optional sidechain. Detector: peak, attack/release one-pole.
namespace dyn {
enum { kThresh, kRatio, kAtk, kRel, kMakeup };
const ParamSpec kParams[] = {
    {"thresh", "THR", -40.f, 0.f, -12.f, Curve::Lin, Unit::Db, nullptr},
    {"ratio", "RAT", 1.f, 20.f, 4.f, Curve::Log, Unit::Ratio, nullptr},
    {"atk", "ATK", 0.1f, 100.f, 5.f, Curve::Log, Unit::Ms, nullptr},
    {"rel", "REL", 10.f, 1000.f, 120.f, Curve::Log, Unit::Ms, nullptr},
    {"makeup", "MKP", 0.f, 24.f, 3.f, Curve::Lin, Unit::Db, nullptr},
};
const char* const kInKeys[] = {"in", "side"};
const char* const kInLabels[] = {"IN", "SID"};
const char* const kOutKeys[] = {"out", "gr"};
const char* const kOutLabels[] = {"OUT", "GR"};
class Dyn : public Module {
   public:
    Dyn() : Module(kDynType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* sd = inOrNull(ins, c, 1);
        const float atk = 1.f - expf(-1.f / (param(kAtk) * 0.001f * kSampleRate));
        const float rel = 1.f - expf(-1.f / (param(kRel) * 0.001f * kSampleRate));
        const float th = param(kThresh), slope = 1.f - 1.f / param(kRatio);
        const float makeup = powf(10.f, param(kMakeup) / 20.f);
        float* o = out(0);
        float* gr = out(1);
        for (int i = 0; i < kBlock; i++) {
            const float x = in ? in[i] : 0.f;
            const float det = fabsf(sd ? sd[i] : x) * (1.f / kAudioVolts);
            env_ += (det > env_ ? atk : rel) * (det - env_);
            if ((i & 3) == 0) {
                const float db = 20.f * log10f(env_ + 1e-6f);
                const float over = db - th;
                gainDb_ = over > 0.f ? -over * slope : 0.f;
                gain_ = powf(10.f, gainDb_ / 20.f);
            }
            o[i] = x * gain_ * makeup;
            gr[i] = -gainDb_ * (kAudioVolts / 24.f);  // 0..5 V for 0..24 dB of reduction
        }
    }

   private:
    float env_ = 0.f, gain_ = 1.f, gainDb_ = 0.f;
};
Module* create() { return new Dyn(); }
}  // namespace dyn

const ModuleTypeInfo kDynType = {
    "dyn", "DYN", "Compressor, sidechain in", Category::Processor, Icon::Drive, Drawing::None,
    5, dyn::kParams, 2, dyn::kInKeys, dyn::kInLabels, 2, dyn::kOutKeys, dyn::kOutLabels,
    580, sizeof(dyn::Dyn) + 2 * kBlock * sizeof(float), 0, dyn::create};

// ---------------------------------------------------------------- AWH
namespace awh {
enum { kWah, kMix, kLevel };
const ParamSpec kParams[] = {
    {"wah", "WAH", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"level", "LVL", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
};
class Awh : public Module {
   public:
    Awh() : Module(kAwhType) { w_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        w_.SetWah(param(kWah));
        w_.SetDryWet(param(kMix) * 100.f);
        w_.SetLevel(param(kLevel));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) o[i] = clampf(w_.Process(in ? in[i] * 0.2f : 0.f), -2.f, 2.f) * kAudioVolts;
    }

   private:
    daisysp::Autowah w_;
};
Module* create() { return new Awh(); }
}  // namespace awh

const ModuleTypeInfo kAwhType = {
    "awh", "AWH", "Auto-wah", Category::Effect, Icon::Fx, Drawing::None,
    3, awh::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    870, sizeof(awh::Awh) + kBlock * sizeof(float), 0, awh::create};

// ---------------------------------------------------------------- ENF
// Envelope follower: |in| through attack/release, x GAIN; GATE is high while it is over THRESH.
namespace enf {
enum { kAtk, kRel, kGain, kThresh };
const ParamSpec kParams[] = {
    {"atk", "ATK", 0.5f, 200.f, 5.f, Curve::Log, Unit::Ms, nullptr},
    {"rel", "REL", 5.f, 2000.f, 150.f, Curve::Log, Unit::Ms, nullptr},
    {"gain", "GAN", 0.25f, 8.f, 1.f, Curve::Log, Unit::Ratio, nullptr},
    {"thresh", "THR", 0.f, 5.f, 1.f, Curve::Lin, Unit::Volts, nullptr},
};
const char* const kOutKeys[] = {"env", "gate"};
const char* const kOutLabels[] = {"ENV", "GAT"};
class Enf : public Module {
   public:
    Enf() : Module(kEnfType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float atk = 1.f - expf(-1.f / (param(kAtk) * 0.001f * kSampleRate));
        const float rel = 1.f - expf(-1.f / (param(kRel) * 0.001f * kSampleRate));
        const float g = param(kGain), th = param(kThresh);
        for (int i = 0; i < kBlock; i++) {
            const float x = in ? fabsf(in[i]) : 0.f;
            env_ += (x > env_ ? atk : rel) * (x - env_);
            const float e = clampf(env_ * g, 0.f, 10.f);
            out(0)[i] = e;
            out(1)[i] = e > th ? kGateHighVolts : 0.f;
        }
    }

   private:
    float env_ = 0.f;
};
Module* create() { return new Enf(); }
}  // namespace enf

const ModuleTypeInfo kEnfType = {
    "enf", "ENF", "Envelope follower + gate", Category::Modulator, Icon::Env, Drawing::Picture,
    4, enf::kParams, 1, kInOnly, kInLbl, 2, enf::kOutKeys, enf::kOutLabels,
    160, sizeof(enf::Enf) + 2 * kBlock * sizeof(float), 0, enf::create};

// ---------------------------------------------------------------- SLW
// Slew limiter: the output moves towards IN at most 10 V per RISE (up) or FALL (down) ms.
namespace slw {
enum { kRise, kFall };
const ParamSpec kParams[] = {
    {"rise", "RIS", 0.f, 5000.f, 100.f, Curve::Sqr, Unit::Ms, nullptr},
    {"fall", "FAL", 0.f, 5000.f, 100.f, Curve::Sqr, Unit::Ms, nullptr},
};
class Slw : public Module {
   public:
    Slw() : Module(kSlwType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float up = param(kRise) < 0.05f ? 1e9f : 10.f / (param(kRise) * 0.001f * kSampleRate);
        const float dn = param(kFall) < 0.05f ? 1e9f : 10.f / (param(kFall) * 0.001f * kSampleRate);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float x = at(in, i), d = x - y_;
            y_ += d > 0.f ? (d < up ? d : up) : (-d < dn ? d : -dn);
            o[i] = y_;
        }
    }

   private:
    float y_ = 0.f;
};
Module* create() { return new Slw(); }
}  // namespace slw

const ModuleTypeInfo kSlwType = {
    "slw", "SLW", "Slew limiter, rise and fall", Category::Utility, Icon::Slew, Drawing::Picture,
    2, slw::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    100, sizeof(slw::Slw) + kBlock * sizeof(float), 0, slw::create};
