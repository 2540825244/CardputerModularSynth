#include "src.h"
#include <cmath>
#include "Drums/analogbassdrum.h"
#include "Drums/analogsnaredrum.h"
#include "Drums/hihat.h"
#include "Noise/particle.h"
#include "PhysicalModeling/modalvoice.h"
#include "PhysicalModeling/resonator.h"
#include "PhysicalModeling/stringvoice.h"
#include "Synthesis/fm2.h"
#include "Synthesis/formantosc.h"
#include "Synthesis/harmonic_osc.h"
#include "Synthesis/variableshapeosc.h"
#include "Synthesis/vosim.h"
#include "Synthesis/zoscillator.h"
#include "common.h"

using namespace mods;

namespace {
const char* const kOutOnly[] = {"out"};
const char* const kOutLbl[] = {"OUT"};

// Shared pitch params: OCT (-4..+4) and TUNE (+-12 semitones) on top of the 1 V/oct input.
#define PITCH_PARAMS                                                                    \
    {"oct", "OCT", -4.f, 4.f, 0.f, Curve::Stepped, Unit::Oct, nullptr},                 \
    {                                                                                   \
        "tune", "TUN", -12.f, 12.f, 0.f, Curve::Lin, Unit::Semi, nullptr                \
    }
inline float baseVolts(const Module& m, int octIdx) { return m.param(octIdx) + m.param(octIdx + 1) * (1.f / 12.f); }

// Trigger in: a rising edge.
struct Trig {
    GateDetector g;
    bool operator()(const float* in, int i) {
        if (!in) return false;
        g.process(in[i]);
        return g.rose;
    }
};
}  // namespace

// ---------------------------------------------------------------- NOI
namespace noi {
enum { kKind, kRate };
const char* const kKinds[] = {"WHT", "PNK", "BRN", "S&H", "DST", "CRK"};
const ParamSpec kParams[] = {
    {"kind", "TYP", 0.f, 5.f, 0.f, Curve::Stepped, Unit::Enum, kKinds},
    {"rate", "RAT", 1.f, 20000.f, 1000.f, Curve::Log, Unit::Hz, nullptr},
};
const char* const kInKeys[] = {"rate"};
const char* const kInLabels[] = {"RAT"};
class Noi : public Module {
   public:
    Noi() : Module(kNoiType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* rc = inOrNull(ins, c, 0);
        const int kind = (int)param(kKind);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float w = rng_.bi();
            const float rate = param(kRate) * fastExp2(at(rc, i)) / kSampleRate;
            float y;
            switch (kind) {
                case 1:  // pink (Paul Kellet's economy filter)
                    b0_ = 0.99765f * b0_ + w * 0.0990460f;
                    b1_ = 0.96300f * b1_ + w * 0.2965164f;
                    b2_ = 0.57000f * b2_ + w * 1.0526913f;
                    y = (b0_ + b1_ + b2_ + w * 0.1848f) * 0.25f;
                    break;
                case 2: y = (brown_ = clampf(brown_ * 0.998f + w * 0.05f, -1.f, 1.f)); break;
                case 3:  // sample & hold noise at RATE
                    ph_ += rate;
                    if (ph_ >= 1.f) {
                        ph_ -= floorf(ph_);
                        held_ = w;
                    }
                    y = held_;
                    break;
                case 4: y = rng_.uni() < rate ? (w > 0.f ? 1.f : -1.f) : 0.f; break;  // dust
                default:
                    if (kind == 5) {  // crackle: sparse, filtered dust
                        const float imp = rng_.uni() < rate * 0.2f ? w : 0.f;
                        crk_ = crk_ * 0.9f + imp;
                        y = clampf(crk_, -1.f, 1.f);
                    } else {
                        y = w;
                    }
                    break;
            }
            o[i] = y * kAudioVolts;
        }
    }

   private:
    Rng rng_;
    float b0_ = 0.f, b1_ = 0.f, b2_ = 0.f, brown_ = 0.f, ph_ = 0.f, held_ = 0.f, crk_ = 0.f;
};
Module* create() { return new Noi(); }
}  // namespace noi

const ModuleTypeInfo kNoiType = {
    "noi", "NOI", "Noise: white pink brown S&H dust", Category::Source, Icon::Noise, Drawing::None,
    2, noi::kParams, 1, noi::kInKeys, noi::kInLabels, 1, kOutOnly, kOutLbl,
    360, sizeof(noi::Noi) + kBlock * sizeof(float), 0, noi::create};

// ---------------------------------------------------------------- VSO
namespace vso {
enum { kOct, kTune, kShape, kPw, kSync, kRatio };
const char* const kOffOn[] = {"OFF", "ON"};
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"shape", "SHP", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
    {"pw", "PW", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"sync", "SYN", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kOffOn},
    {"ratio", "RAT", 1.f, 8.f, 2.f, Curve::Log, Unit::Ratio, nullptr},
};
const char* const kInKeys[] = {"pitch", "fm", "shape", "pw"};
const char* const kInLabels[] = {"PIT", "FM", "SHP", "PW"};
class Vso : public Module {
   public:
    Vso() : Module(kVsoType) { o_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* fm = inOrNull(ins, c, 1);
        const float* sh = inOrNull(ins, c, 2);
        const float* pw = inOrNull(ins, c, 3);
        const float base = baseVolts(*this, kOct);
        const bool sync = param(kSync) >= 0.5f;
        o_.SetSync(sync);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float hz = hzClamp(voltsToHz(base + at(pit, i) + at(fm, i)));
            // DaisySP: the audible oscillator is the slave (SetSyncFreq); SetFreq is the master
            // that resets it. Set before SetPW, which clamps by the slave frequency.
            if (sync) {
                o_.SetFreq(hz);
                o_.SetSyncFreq(hzClamp(hz * param(kRatio)));
            } else {
                o_.SetSyncFreq(hz);
            }
            o_.SetWaveshape(clampf(param(kShape) + at(sh, i) * 0.2f, 0.f, 1.f));
            o_.SetPW(clampf(param(kPw) + at(pw, i) * 0.1f, 0.02f, 0.98f));
            o[i] = clampf(o_.Process(), -1.2f, 1.2f) * kAudioVolts;
        }
    }

   private:
    daisysp::VariableShapeOscillator o_;
};
Module* create() { return new Vso(); }
}  // namespace vso

const ModuleTypeInfo kVsoType = {
    "vso", "VSO", "Saw-square morph, PW, sync", Category::Source, Icon::Saw, Drawing::None,
    6, vso::kParams, 4, vso::kInKeys, vso::kInLabels, 1, kOutOnly, kOutLbl,
    770, sizeof(vso::Vso) + kBlock * sizeof(float), 0, vso::create};

// ---------------------------------------------------------------- FMO
namespace fmo {
enum { kOct, kTune, kRatio, kIndex };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"ratio", "RAT", 0.25f, 8.f, 2.f, Curve::Log, Unit::Ratio, nullptr},
    {"index", "IDX", 0.f, 1.f, 0.3f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "index"};
const char* const kInLabels[] = {"PIT", "IDX"};
class Fmo : public Module {
   public:
    Fmo() : Module(kFmoType) { o_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* ix = inOrNull(ins, c, 1);
        const float base = baseVolts(*this, kOct);
        o_.SetRatio(param(kRatio));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            if ((i & 3) == 0) {
                o_.SetFrequency(hzClamp(voltsToHz(base + at(pit, i))));
                o_.SetIndex(clampf(param(kIndex) + at(ix, i) * 0.2f, 0.f, 1.f) * 8.f);
            }
            o[i] = o_.Process() * kAudioVolts;
        }
    }

   private:
    daisysp::Fm2 o_;
};
Module* create() { return new Fmo(); }
}  // namespace fmo

const ModuleTypeInfo kFmoType = {
    "fmo", "FMO", "2-op FM: ratio, index CV", Category::Source, Icon::Fm, Drawing::None,
    4, fmo::kParams, 2, fmo::kInKeys, fmo::kInLabels, 1, kOutOnly, kOutLbl,
    980, sizeof(fmo::Fmo) + kBlock * sizeof(float), 0, fmo::create};

// ---------------------------------------------------------------- FOR
namespace fmt {
enum { kOct, kTune, kFormant, kShift };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"formant", "FRM", 100.f, 6000.f, 800.f, Curve::Log, Unit::Hz, nullptr},
    {"shift", "PHS", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "formant"};
const char* const kInLabels[] = {"PIT", "FRM"};
class For : public Module {
   public:
    For() : Module(kForType) { o_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* fr = inOrNull(ins, c, 1);
        const float base = baseVolts(*this, kOct);
        o_.SetPhaseShift(param(kShift));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            o_.SetCarrierFreq(hzClamp(voltsToHz(base + at(pit, i))));
            o_.SetFormantFreq(hzClamp(param(kFormant) * fastExp2(at(fr, i)), 50.f, 12000.f));
            o[i] = o_.Process() * kAudioVolts;
        }
    }

   private:
    daisysp::FormantOscillator o_;
};
Module* create() { return new For(); }
}  // namespace fmt

const ModuleTypeInfo kForType = {
    "for", "FOR", "Formant oscillator", Category::Source, Icon::Osc, Drawing::None,
    4, fmt::kParams, 2, fmt::kInKeys, fmt::kInLabels, 1, kOutOnly, kOutLbl,
    740, sizeof(fmt::For) + kBlock * sizeof(float), 0, fmt::create};

// ---------------------------------------------------------------- SSW
// Supersaw: 7 polyBLEP saws spread by DETUNE; MIX balances the centre saw against the six others.
namespace ssw {
enum { kOct, kTune, kDetune, kMix };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"detune", "DET", 0.f, 1.f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "detune"};
const char* const kInLabels[] = {"PIT", "DET"};
const float kSpread[7] = {0.f, -0.11002f, 0.10929f, -0.06288f, 0.06123f, -0.01953f, 0.01953f};
inline float blep(float t, float dt, float idt) {  // idt = 1 / dt
    if (t < dt) {
        t *= idt;
        return t + t - t * t - 1.f;
    }
    if (t > 1.f - dt) {
        t = (t - 1.f) * idt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}
class Ssw : public Module {
   public:
    Ssw() : Module(kSswType) {
        Rng r;
        for (float& p : ph_) p = r.uni();
    }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* dc = inOrNull(ins, c, 1);
        const float base = baseVolts(*this, kOct), mix = param(kMix);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float hz = hzClamp(voltsToHz(base + at(pit, i)), 1.f, 12000.f);
            const float det = clampf(param(kDetune) + at(dc, i) * 0.2f, 0.f, 1.f);
            if (hz != lastHz_ || det != lastDet_) {  // steps and their reciprocals, on a change only
                lastHz_ = hz;
                lastDet_ = det;
                for (int v = 0; v < 7; v++) {
                    dt_[v] = hz * (1.f + kSpread[v] * det * det) * (1.f / kSampleRate);
                    idt_[v] = recipPos(dt_[v]);
                }
            }
            float y = 0.f;
            for (int v = 0; v < 7; v++) {
                const float dt = dt_[v];
                ph_[v] += dt;
                if (ph_[v] >= 1.f) ph_[v] -= 1.f;
                const float s = 2.f * ph_[v] - 1.f - blep(ph_[v], dt, idt_[v]);
                y += v == 0 ? s * (1.f - 0.5f * mix) : s * mix * 0.3f;
            }
            o[i] = clampf(y * 0.6f, -1.5f, 1.5f) * kAudioVolts;
        }
    }

   private:
    float ph_[7];
    float dt_[7], idt_[7];
    float lastHz_ = -1.f, lastDet_ = -1.f;
};
Module* create() { return new Ssw(); }
}  // namespace ssw

const ModuleTypeInfo kSswType = {
    "ssw", "SSW", "Supersaw, 7 detuned saws", Category::Source, Icon::Saw, Drawing::None,
    4, ssw::kParams, 2, ssw::kInKeys, ssw::kInLabels, 1, kOutOnly, kOutLbl,
    1050, sizeof(ssw::Ssw) + kBlock * sizeof(float), 0, ssw::create};

// ---------------------------------------------------------------- STR / MDL
// String and modal voices (Rings-style, from DaisySP): TRIG strikes, PITCH tunes, ACC in adds accent.
namespace phys {
enum { kOct, kTune, kStruct, kBright, kDamp, kAccent };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"struct", "STR", 0.f, 1.f, 0.3f, Curve::Lin, Unit::Percent, nullptr},
    {"bright", "BRI", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"damp", "DMP", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"accent", "ACC", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"trig", "pitch", "acc", "bright"};
const char* const kInLabels[] = {"TRG", "PIT", "ACC", "BRI"};
template <typename Voice, const ModuleTypeInfo& T>
class Phys : public Module {
   public:
    explicit Phys(float gain) : Module(T), gain_(gain) { v_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float* pit = inOrNull(ins, c, 1);
        const float* acc = inOrNull(ins, c, 2);
        const float* br = inOrNull(ins, c, 3);
        const float base = baseVolts(*this, kOct);
        v_.SetStructure(param(kStruct));
        v_.SetDamping(param(kDamp));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const bool t = trig_(tr, i);
            if (t || i == 0) {
                v_.SetFreq(hzClamp(voltsToHz(base + at(pit, i)), 20.f, 8000.f));
                v_.SetBrightness(clampf(param(kBright) + at(br, i) * 0.2f, 0.f, 1.f));
                v_.SetAccent(clampf(param(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
            }
            o[i] = clampf(v_.Process(t) * gain_, -2.f, 2.f) * kAudioVolts;
        }
    }

   private:
    Voice v_;
    Trig trig_;
    float gain_;
};
}  // namespace phys

namespace str {
Module* create() { return new phys::Phys<daisysp::StringVoice, kStrType>(1.2f); }
}
namespace mdl {
Module* create() { return new phys::Phys<daisysp::ModalVoice, kMdlType>(0.5f); }
}

const ModuleTypeInfo kStrType = {
    "str", "STR", "Plucked string (Karplus)", Category::Source, Icon::String, Drawing::None,
    6, phys::kParams, 4, phys::kInKeys, phys::kInLabels, 1, kOutOnly, kOutLbl,
    1050, sizeof(phys::Phys<daisysp::StringVoice, kStrType>) + kBlock * sizeof(float), 0, str::create};
const ModuleTypeInfo kMdlType = {
    "mdl", "MDL", "Modal voice: bells, bars", Category::Source, Icon::Bell, Drawing::None,
    6, phys::kParams, 4, phys::kInKeys, phys::kInLabels, 1, kOutOnly, kOutLbl,
    1460, sizeof(phys::Phys<daisysp::ModalVoice, kMdlType>) + kBlock * sizeof(float), 0, mdl::create};

// ---------------------------------------------------------------- RES
// Modal resonator excited by IN (24 modes, Rings' resonator).
namespace res {
enum { kOct, kTune, kStruct, kBright, kDamp };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"struct", "STR", 0.f, 1.f, 0.25f, Curve::Lin, Unit::Percent, nullptr},
    {"bright", "BRI", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"damp", "DMP", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "pitch"};
const char* const kInLabels[] = {"IN", "PIT"};
class Res : public Module {
   public:
    Res() : Module(kResType) { r_.Init(0.015f, 16, kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* pit = inOrNull(ins, c, 1);
        r_.SetFreq(hzClamp(voltsToHz(baseVolts(*this, kOct) + at(pit, 0)), 20.f, 8000.f));
        r_.SetStructure(param(kStruct));
        r_.SetBrightness(param(kBright));
        r_.SetDamping(param(kDamp));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) o[i] = clampf(r_.Process(in ? in[i] * 0.2f : 0.f), -2.f, 2.f) * kAudioVolts;
    }

   private:
    daisysp::Resonator r_;
};
Module* create() { return new Res(); }
}  // namespace res

const ModuleTypeInfo kResType = {
    "res", "RES", "Modal resonator for any input", Category::Processor, Icon::Bell, Drawing::None,
    5, res::kParams, 2, res::kInKeys, res::kInLabels, 1, kOutOnly, kOutLbl,
    940, sizeof(res::Res) + kBlock * sizeof(float), 0, res::create};

// ---------------------------------------------------------------- KCK / SNR / HAT
namespace drum {
const char* const kInKeys[] = {"trig", "acc", "pitch"};
const char* const kInLabels[] = {"TRG", "ACC", "PIT"};
}  // namespace drum

namespace kck {
enum { kFreq, kTone, kDecay, kAtkFm, kSelfFm, kAccent };
const ParamSpec kParams[] = {
    {"freq", "FRQ", 30.f, 150.f, 55.f, Curve::Log, Unit::Hz, nullptr},
    {"tone", "TON", 0.f, 1.f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"decay", "DEC", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"atkfm", "AFM", 0.f, 1.f, 0.3f, Curve::Lin, Unit::Percent, nullptr},
    {"selffm", "SFM", 0.f, 1.f, 0.2f, Curve::Lin, Unit::Percent, nullptr},
    {"accent", "ACC", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
};
class Kck : public Module {
   public:
    Kck() : Module(kKckType) { d_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float* acc = inOrNull(ins, c, 1);
        const float* pit = inOrNull(ins, c, 2);
        d_.SetTone(param(kTone));
        d_.SetDecay(param(kDecay));
        d_.SetAttackFmAmount(param(kAtkFm));
        d_.SetSelfFmAmount(param(kSelfFm));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const bool t = trig_(tr, i);
            if (t) {
                d_.SetFreq(clampf(param(kFreq) * fastExp2(at(pit, i)), 20.f, 400.f));
                d_.SetAccent(clampf(param(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
            }
            o[i] = clampf(d_.Process(t) * 3.4f, -2.f, 2.f) * kAudioVolts;  // ~5 V peaks
        }
    }

   private:
    daisysp::AnalogBassDrum d_;
    Trig trig_;
};
Module* create() { return new Kck(); }
}  // namespace kck

const ModuleTypeInfo kKckType = {
    "kck", "KCK", "Analog kick drum", Category::Source, Icon::Drum, Drawing::None,
    6, kck::kParams, 3, drum::kInKeys, drum::kInLabels, 1, kOutOnly, kOutLbl,
    730, sizeof(kck::Kck) + kBlock * sizeof(float), 0, kck::create};

namespace snr {
enum { kFreq, kTone, kDecay, kSnappy, kAccent };
const ParamSpec kParams[] = {
    {"freq", "FRQ", 100.f, 400.f, 200.f, Curve::Log, Unit::Hz, nullptr},
    {"tone", "TON", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"decay", "DEC", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"snappy", "SNP", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"accent", "ACC", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
};
class Snr : public Module {
   public:
    Snr() : Module(kSnrType) { d_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float* acc = inOrNull(ins, c, 1);
        const float* pit = inOrNull(ins, c, 2);
        d_.SetTone(param(kTone));
        d_.SetDecay(param(kDecay));
        d_.SetSnappy(param(kSnappy));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const bool t = trig_(tr, i);
            if (t) {
                d_.SetFreq(clampf(param(kFreq) * fastExp2(at(pit, i)), 50.f, 700.f));  // NaN above ~1 kHz
                d_.SetAccent(clampf(param(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
            }
            o[i] = clampf(d_.Process(t) * 0.7f, -2.f, 2.f) * kAudioVolts;
        }
    }

   private:
    daisysp::AnalogSnareDrum d_;
    Trig trig_;
};
Module* create() { return new Snr(); }
}  // namespace snr

const ModuleTypeInfo kSnrType = {
    "snr", "SNR", "Analog snare drum", Category::Source, Icon::Drum, Drawing::None,
    5, snr::kParams, 3, drum::kInKeys, drum::kInLabels, 1, kOutOnly, kOutLbl,
    950, sizeof(snr::Snr) + kBlock * sizeof(float), 0, snr::create};

namespace hat {
enum { kFreq, kTone, kDecay, kNoise, kAccent };
const ParamSpec kParams[] = {
    {"freq", "FRQ", 1500.f, 12000.f, 4000.f, Curve::Log, Unit::Hz, nullptr},
    {"tone", "TON", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"decay", "DEC", 0.f, 1.f, 0.3f, Curve::Lin, Unit::Percent, nullptr},
    {"noise", "NOI", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"accent", "ACC", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
};
class Hat : public Module {
   public:
    Hat() : Module(kHatType) { d_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float* acc = inOrNull(ins, c, 1);
        const float* pit = inOrNull(ins, c, 2);
        d_.SetTone(param(kTone));
        d_.SetDecay(param(kDecay));
        d_.SetNoisiness(param(kNoise));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const bool t = trig_(tr, i);
            if (t) {
                d_.SetFreq(clampf(param(kFreq) * fastExp2(at(pit, i)), 500.f, 16000.f));
                d_.SetAccent(clampf(param(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
            }
            o[i] = clampf(d_.Process(t) * 4.5f, -2.f, 2.f) * kAudioVolts;
        }
    }

   private:
    daisysp::HiHat<> d_;
    Trig trig_;
};
Module* create() { return new Hat(); }
}  // namespace hat

const ModuleTypeInfo kHatType = {
    "hat", "HAT", "Metallic hi-hat", Category::Source, Icon::Hat, Drawing::None,
    5, hat::kParams, 3, drum::kInKeys, drum::kInLabels, 1, kOutOnly, kOutLbl,
    750, sizeof(hat::Hat) + kBlock * sizeof(float), 0, hat::create};

// ---------------------------------------------------------------- PRT
namespace prt {
enum { kFreq, kRes, kDensity, kSpread, kGain };
const ParamSpec kParams[] = {
    {"freq", "FRQ", 50.f, 10000.f, 1500.f, Curve::Log, Unit::Hz, nullptr},
    {"res", "RES", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
    {"density", "DEN", 0.f, 1.f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"spread", "SPR", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"gain", "GAN", 0.f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"density", "pitch"};
const char* const kInLabels[] = {"DEN", "PIT"};
class Prt : public Module {
   public:
    Prt() : Module(kPrtType) { p_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* dn = inOrNull(ins, c, 0);
        const float* pit = inOrNull(ins, c, 1);
        p_.SetFreq(clampf(param(kFreq) * fastExp2(at(pit, 0)), 20.f, 16000.f));
        p_.SetResonance(param(kRes) * 0.99f);
        p_.SetDensity(clampf(param(kDensity) + at(dn, 0) * 0.2f, 0.f, 1.f) * 0.5f);
        p_.SetSpread(param(kSpread) * 4.f);
        p_.SetGain(param(kGain));
        p_.SetRandomFreq(0.f);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) o[i] = clampf(p_.Process(), -2.f, 2.f) * kAudioVolts;
    }

   private:
    daisysp::Particle p_;
};
Module* create() { return new Prt(); }
}  // namespace prt

const ModuleTypeInfo kPrtType = {
    "prt", "PRT", "Particles: resonant dust", Category::Source, Icon::Noise, Drawing::None,
    5, prt::kParams, 2, prt::kInKeys, prt::kInLabels, 1, kOutOnly, kOutLbl,
    410, sizeof(prt::Prt) + kBlock * sizeof(float), 0, prt::create};

// ---------------------------------------------------------------- VOS
namespace vos {
enum { kOct, kTune, kForm1, kForm2, kShape };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"form1", "F1", 100.f, 4000.f, 600.f, Curve::Log, Unit::Hz, nullptr},
    {"form2", "F2", 100.f, 6000.f, 1800.f, Curve::Log, Unit::Hz, nullptr},
    {"shape", "SHP", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "form"};
const char* const kInLabels[] = {"PIT", "FRM"};
class Vos : public Module {
   public:
    Vos() : Module(kVosType) { o_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* fr = inOrNull(ins, c, 1);
        const float base = baseVolts(*this, kOct);
        o_.SetShape(param(kShape) * 2.f - 1.f);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float f = fastExp2(at(fr, i));
            o_.SetFreq(hzClamp(voltsToHz(base + at(pit, i))));
            o_.SetForm1Freq(hzClamp(param(kForm1) * f));
            o_.SetForm2Freq(hzClamp(param(kForm2) * f));
            o[i] = clampf(o_.Process(), -1.5f, 1.5f) * kAudioVolts;
        }
    }

   private:
    daisysp::VosimOscillator o_;
};
Module* create() { return new Vos(); }
}  // namespace vos

const ModuleTypeInfo kVosType = {
    "vos", "VOS", "VOSIM: two formants", Category::Source, Icon::Osc, Drawing::None,
    5, vos::kParams, 2, vos::kInKeys, vos::kInLabels, 1, kOutOnly, kOutLbl,
    1030, sizeof(vos::Vos) + kBlock * sizeof(float), 0, vos::create};

// ---------------------------------------------------------------- ZOS
namespace zos {
enum { kOct, kTune, kFormant, kShape, kMode };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"formant", "FRM", 100.f, 6000.f, 1000.f, Curve::Log, Unit::Hz, nullptr},
    {"shape", "SHP", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"mode", "MOD", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "shape"};
const char* const kInLabels[] = {"PIT", "SHP"};
class Zos : public Module {
   public:
    Zos() : Module(kZosType) { o_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* sh = inOrNull(ins, c, 1);
        const float base = baseVolts(*this, kOct);
        o_.SetFormantFreq(param(kFormant));
        o_.SetMode(param(kMode) * 2.f - 1.f);
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            o_.SetFreq(hzClamp(voltsToHz(base + at(pit, i))));
            o_.SetShape(clampf(param(kShape) + at(sh, i) * 0.2f, 0.f, 1.f));
            o[i] = clampf(o_.Process(), -1.5f, 1.5f) * kAudioVolts;
        }
    }

   private:
    daisysp::ZOscillator o_;
};
Module* create() { return new Zos(); }
}  // namespace zos

const ModuleTypeInfo kZosType = {
    "zos", "ZOS", "Z oscillator: formant sync", Category::Source, Icon::Osc, Drawing::None,
    5, zos::kParams, 2, zos::kInKeys, zos::kInLabels, 1, kOutOnly, kOutLbl,
    900, sizeof(zos::Zos) + kBlock * sizeof(float), 0, zos::create};

// ---------------------------------------------------------------- HRM
// Additive: 16 harmonics, amplitudes peaked at CENTRE, falling by ROLLOFF per harmonic of distance.
namespace hrm {
enum { kOct, kTune, kCenter, kRolloff, kOdd };
const ParamSpec kParams[] = {
    PITCH_PARAMS,
    {"center", "CEN", 1.f, 16.f, 1.f, Curve::Lin, Unit::Int, nullptr},
    {"rolloff", "ROL", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"odd", "ODD", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "center"};
const char* const kInLabels[] = {"PIT", "CEN"};
class Hrm : public Module {
   public:
    Hrm() : Module(kHrmType) { o_.Init(kSampleRate); }
    void process(const float* const* ins, const bool* c) override {
        const float* pit = inOrNull(ins, c, 0);
        const float* cc = inOrNull(ins, c, 1);
        const float center = clampf(param(kCenter) + at(cc, 0) * 3.f, 1.f, 16.f);
        const float roll = param(kRolloff), odd = param(kOdd);
        if (center != lastC_ || roll != lastR_ || odd != lastO_) {
            lastC_ = center;
            lastR_ = roll;
            lastO_ = odd;
            float amp[16], sum = 0.f;
            for (int h = 0; h < 16; h++) {
                const float d = fabsf((float)(h + 1) - center);
                amp[h] = powf(roll, d) * (((h + 1) % 2 == 0) ? 1.f - odd : 1.f);
                sum += amp[h];
            }
            for (float& a : amp) a /= sum > 0.f ? sum : 1.f;
            o_.SetAmplitudes(amp);
        }
        const float base = baseVolts(*this, kOct);
        o_.SetFreq(hzClamp(voltsToHz(base + at(pit, 0)), 10.f, 1200.f));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) o[i] = clampf(o_.Process(), -1.5f, 1.5f) * kAudioVolts;
    }

   private:
    daisysp::HarmonicOscillator<16> o_;
    float lastC_ = -1.f, lastR_ = -1.f, lastO_ = -1.f;
};
Module* create() { return new Hrm(); }
}  // namespace hrm

const ModuleTypeInfo kHrmType = {
    "hrm", "HRM", "Additive, 16 harmonics", Category::Source, Icon::Osc, Drawing::None,
    5, hrm::kParams, 2, hrm::kInKeys, hrm::kInLabels, 1, kOutOnly, kOutLbl,
    660, sizeof(hrm::Hrm) + kBlock * sizeof(float), 0, hrm::create};
