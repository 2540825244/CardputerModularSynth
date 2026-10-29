#include "v14.h"
#include <cmath>
#include "../biquad.h"
#include "common.h"

using namespace mods;

namespace {
const char* const kOutOnly[] = {"out"};
const char* const kOutLbl[] = {"OUT"};
const char* const kInOnly[] = {"in"};
const char* const kInLbl[] = {"IN"};

// sin(2 pi p), p in [0, 1): parabola + one refinement, error < 1e-3
inline float fsin(float p) {
    const float q = p < 0.5f ? p : p - 1.f;
    const float y = 8.f * q * (1.f - 2.f * (q < 0.f ? -q : q));
    return 0.225f * (y * (y < 0.f ? -y : y) - y) + y;
}
// Per-sample decay factor for a time constant of `ms`.
inline float decayK(float ms) { return expf(-1.f / (ms * 0.001f * kSampleRate)); }

// Chamberlin state-variable band-pass, cheap enough to run three of.
struct Svf {
    float lp = 0.f, bp = 0.f, f = 0.1f, q = 0.5f;
    void set(float hz, float damp) {
        f = 2.f * sinf(3.1415927f * clampf(hz, 20.f, 9000.f) / kSampleRate);
        q = damp;
    }
    float band(float x) {
        lp += f * bp;
        const float hp = x - lp - q * bp;
        bp += f * hp;
        return bp;
    }
};
}  // namespace

// ---------------------------------------------------------------- ARP
// Arpeggiator: steps through a chord over OCT octaves above PIT on each CLK rise (or at RAT with
// CLK unpatched) while GATE is high (always when GATE is unpatched); a new gate starts at the
// root. GAT out follows CLK (or GLN of the RAT period).
namespace arp {
enum { kChord, kMode, kOct, kRate, kGateLen };
const char* const kChords[] = {"MAJ", "MIN", "7TH", "M7", "SUS", "5TH", "OCT"};
const char* const kModes[] = {"UP", "DN", "U/D", "RND"};
const ParamSpec kParams[] = {
    {"chord", "CHD", 0.f, 6.f, 0.f, Curve::Stepped, Unit::Enum, kChords},
    {"mode", "MOD", 0.f, 3.f, 0.f, Curve::Stepped, Unit::Enum, kModes},
    {"oct", "OCT", 1.f, 3.f, 1.f, Curve::Stepped, Unit::Int, nullptr},
    {"rate", "RAT", 0.5f, 20.f, 6.f, Curve::Log, Unit::Hz, nullptr},
    {"gatelen", "GLN", 0.05f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"clk", "pitch", "gate"};
const char* const kInLabels[] = {"CLK", "PIT", "GAT"};
const char* const kOutKeys[] = {"pitch", "gate"};
const char* const kOutLabels[] = {"PIT", "GAT"};
const int kChordNotes[7][4] = {{0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11},
                               {0, 5, 7, -1}, {0, 7, -1, -1}, {0, -1, -1, -1}};

class Arp : public Module {
   public:
    Arp() : Module(kArpType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* ck = inOrNull(ins, c, 0);
        const float* pit = inOrNull(ins, c, 1);
        const float* gt = inOrNull(ins, c, 2);
        const int chord = (int)param(kChord), mode = (int)param(kMode), oct = (int)param(kOct);
        if (chord != chord_ || mode != mode_ || oct != oct_) {
            chord_ = chord;
            mode_ = mode;
            oct_ = oct;
            n_ = arpPattern(chord, mode, oct, notes_, 24);
            if (step_ >= n_) step_ = 0;
        }
        const float inc = param(kRate) / kSampleRate, gl = param(kGateLen);
        for (int i = 0; i < kBlock; i++) {
            bool held = true;
            if (gt) {
                gate_.process(gt[i]);
                held = gate_.high;
                if (gate_.rose) {
                    first_ = true;
                    ph_ = 1.f;  // with its own clock, a new gate ticks at once
                }
            }
            bool tick = false, clkHigh = false;
            if (ck) {
                clk_.process(ck[i]);
                tick = clk_.rose;
                clkHigh = clk_.high;
            } else {
                ph_ += inc;
                if (ph_ >= 1.f) {
                    ph_ -= 1.f;
                    tick = true;
                }
                clkHigh = ph_ < gl;
            }
            if (tick && held) {
                if (first_) {
                    step_ = 0;
                    first_ = false;
                } else if (mode_ == 3) {
                    step_ = n_ > 1 ? (int)(rng_.uni() * (float)n_) % n_ : 0;
                } else {
                    step_ = (step_ + 1) % n_;
                }
                note_ = (float)notes_[step_] * (1.f / 12.f);
                playing_ = true;
            }
            if (!held) playing_ = false;
            out(0)[i] = at(pit, i) + note_;
            out(1)[i] = playing_ && held && clkHigh ? kGateHighVolts : 0.f;
        }
        setMonitor(0, (float)step_);
        setMonitor(1, playing_ ? 1.f : 0.f);
    }

   private:
    GateDetector clk_, gate_;
    Rng rng_;
    int notes_[24] = {0};
    int n_ = 1, step_ = 0, chord_ = -1, mode_ = -1, oct_ = -1;
    float ph_ = 1.f, note_ = 0.f;
    bool first_ = true, playing_ = false;
};
Module* create() { return new Arp(); }
}  // namespace arp

int arpPattern(int chord, int mode, int octaves, int* semis, int max) {
    chord = chord < 0 ? 0 : (chord > 6 ? 6 : chord);
    octaves = octaves < 1 ? 1 : (octaves > 3 ? 3 : octaves);
    int up[24], n = 0;
    for (int o = 0; o < octaves; o++)
        for (int k = 0; k < 4 && arp::kChordNotes[chord][k] >= 0 && n < 24; k++) up[n++] = arp::kChordNotes[chord][k] + 12 * o;
    int m = 0;
    if (mode == 1) {
        for (int k = n - 1; k >= 0 && m < max; k--) semis[m++] = up[k];
    } else if (mode == 2) {
        for (int k = 0; k < n && m < max; k++) semis[m++] = up[k];
        for (int k = n - 2; k >= 1 && m < max; k--) semis[m++] = up[k];  // the ends once
    } else {
        for (int k = 0; k < n && m < max; k++) semis[m++] = up[k];
    }
    return m;
}

const ModuleTypeInfo kArpType = {
    "arp", "ARP", "Arpeggio: chords, up/down, octs", Category::Modulator, Icon::Seq, Drawing::Picture,
    5, arp::kParams, 3, arp::kInKeys, arp::kInLabels, 2, arp::kOutKeys, arp::kOutLabels,
    60, sizeof(arp::Arp) + 2 * kBlock * sizeof(float), 0, arp::create};

// ---------------------------------------------------------------- CLP
// Handclap: three noise bursts SPR ms apart, then a tail of DEC, through a band-pass at TON.
namespace clp {
enum { kTone, kDecay, kSpread, kLevel };
const ParamSpec kParams[] = {
    {"tone", "TON", 600.f, 3000.f, 1200.f, Curve::Log, Unit::Hz, nullptr},
    {"decay", "DEC", 40.f, 800.f, 180.f, Curve::Log, Unit::Ms, nullptr},
    {"spread", "SPR", 4.f, 20.f, 9.f, Curve::Lin, Unit::Ms, nullptr},
    {"level", "LVL", 0.f, 1.f, 0.8f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"trig", "accent"};
const char* const kInLabels[] = {"TRG", "ACC"};
class Clp : public Module {
   public:
    Clp() : Module(kClpType) {}
    void silence() override {
        burst_ = tail_ = 0.f;
        t_ = -1;
        bp_.lp = bp_.bp = 0.f;
    }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float* ac = inOrNull(ins, c, 1);
        bp_.set(param(kTone), 0.6f);
        const int spr = (int)(param(kSpread) * 0.001f * kSampleRate);
        const float kBurst = decayK(3.f), kTail = decayK(param(kDecay));
        for (int i = 0; i < kBlock; i++) {
            trig_.process(tr ? tr[i] : 0.f);
            if (trig_.rose) {
                t_ = 0;
                acc_ = clampf(param(kLevel) + at(ac, i) * 0.2f, 0.f, 1.f);
            }
            if (t_ >= 0) {
                if (t_ == 0 || t_ == spr || t_ == 2 * spr) burst_ = 1.f;
                if (t_ == 3 * spr) tail_ = 0.7f;
                if (t_ > 3 * spr + (int)(kSampleRate * 4.f)) t_ = -1;
                else t_++;
            }
            const float env = burst_ + tail_;
            burst_ = burst_ > 1e-6f ? burst_ * kBurst : 0.f;  // flushed: no subnormal tail
            tail_ = tail_ > 1e-6f ? tail_ * kTail : 0.f;
            out(0)[i] = clampf(bp_.band(rng_.bi() * env) * 3.f * acc_, -2.f, 2.f) * kAudioVolts;
        }
    }

   private:
    GateDetector trig_;
    Rng rng_;
    Svf bp_;
    int t_ = -1;
    float burst_ = 0.f, tail_ = 0.f, acc_ = 0.8f;
};
Module* create() { return new Clp(); }
}  // namespace clp

const ModuleTypeInfo kClpType = {
    "clp", "CLP", "Handclap: 3 bursts and a tail", Category::Source, Icon::Drum, Drawing::Picture,
    4, clp::kParams, 2, clp::kInKeys, clp::kInLabels, 1, kOutOnly, kOutLbl,
    220, sizeof(clp::Clp) + kBlock * sizeof(float), 0, clp::create};

// ---------------------------------------------------------------- PRC
// Percussion: TOM and CNG are swept sines, CB the 808's two squares, CLV a short sine, RIM a
// triangle with a click. TUN moves the mode's pitch; DEC its length.
namespace prc {
enum { kMode, kTune, kDecay, kLevel };
const char* const kModes[] = {"TOM", "CNG", "CB", "CLV", "RIM"};
const ParamSpec kParams[] = {
    {"mode", "MOD", 0.f, 4.f, 0.f, Curve::Stepped, Unit::Enum, kModes},
    {"tune", "TUN", -24.f, 24.f, 0.f, Curve::Lin, Unit::Semi, nullptr},
    {"decay", "DEC", 0.f, 1.f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"level", "LVL", 0.f, 1.f, 0.8f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"trig", "accent", "pitch"};
const char* const kInLabels[] = {"TRG", "ACC", "PIT"};
}  // namespace prc

const float kPrcBaseHz[5] = {110.f, 220.f, 540.f, 2500.f, 1700.f};

void prcShape(int mode, float decay, float* sweep, float* bodyMs, float* clickMs) {
    switch (mode) {
        case 1: *sweep = 0.25f; *bodyMs = 50.f + 300.f * decay; *clickMs = 1.f; break;    // conga
        case 2: *sweep = 0.f; *bodyMs = 50.f + 400.f * decay; *clickMs = 15.f; break;     // cowbell: fast part
        case 3: *sweep = 0.f; *bodyMs = 4.f + 30.f * decay; *clickMs = 0.f; break;        // clave
        case 4: *sweep = 0.f; *bodyMs = 3.f + 20.f * decay; *clickMs = 2.f; break;        // rim
        default: *sweep = 0.6f; *bodyMs = 80.f + 600.f * decay; *clickMs = 2.f; break;    // tom
    }
}

namespace prc {
class Prc : public Module {
   public:
    Prc() : Module(kPrcType) {}
    void silence() override { amp_ = fast_ = sw_ = 0.f; }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float* ac = inOrNull(ins, c, 1);
        const float* pit = inOrNull(ins, c, 2);
        int mode = (int)param(kMode);
        mode = mode < 0 ? 0 : (mode > 4 ? 4 : mode);
        float sweep, bodyMs, clickMs;
        prcShape(mode, param(kDecay), &sweep, &bodyMs, &clickMs);
        const float kBody = decayK(bodyMs), kSweep = decayK(mode == 1 ? 8.f : 15.f);
        const float kFast = clickMs > 0.f ? decayK(clickMs) : 0.f;
        for (int i = 0; i < kBlock; i++) {
            trig_.process(tr ? tr[i] : 0.f);
            if (trig_.rose) {
                amp_ = clampf(param(kLevel) + at(ac, i) * 0.2f, 0.f, 1.f);
                fast_ = amp_;
                sw_ = sweep;
                hz_ = clampf(kPrcBaseHz[mode] * fastExp2(param(kTune) * (1.f / 12.f) + at(pit, i)), 20.f, 12000.f);
                ph1_ = ph2_ = 0.f;
            }
            const float f = hz_ * (1.f + sw_) / kSampleRate;
            ph1_ += f;
            ph1_ -= floorf(ph1_);
            float y;
            if (mode == 2) {  // cowbell: 540 and 800 Hz squares, a fast hit then the ring
                ph2_ += f * (800.f / 540.f);
                ph2_ -= floorf(ph2_);
                const float sq = (ph1_ < 0.5f ? 1.f : -1.f) + (ph2_ < 0.5f ? 1.f : -1.f);
                hp_ += 0.2f * (sq - hp_);
                y = (sq - hp_) * 0.35f * (0.6f * fast_ + 0.4f * amp_);
            } else if (mode == 4) {  // rim: a triangle and a noise click
                const float tri = ph1_ < 0.5f ? 4.f * ph1_ - 1.f : 3.f - 4.f * ph1_;
                y = tri * amp_ + rng_.bi() * fast_ * 0.6f;
            } else {
                y = fsin(ph1_) * amp_ + (clickMs > 0.f ? rng_.bi() * fast_ * 0.25f : 0.f);
            }
            amp_ = amp_ > 1e-6f ? amp_ * kBody : 0.f;
            fast_ = fast_ > 1e-6f ? fast_ * kFast : 0.f;
            sw_ = sw_ > 1e-6f ? sw_ * kSweep : 0.f;
            out(0)[i] = clampf(y, -1.5f, 1.5f) * kAudioVolts;
        }
    }

   private:
    GateDetector trig_;
    Rng rng_;
    float amp_ = 0.f, fast_ = 0.f, sw_ = 0.f, hz_ = 110.f, ph1_ = 0.f, ph2_ = 0.f, hp_ = 0.f;
};
Module* create() { return new Prc(); }
}  // namespace prc

const ModuleTypeInfo kPrcType = {
    "prc", "PRC", "Tom conga cowbell clave rim", Category::Source, Icon::Drum, Drawing::Picture,
    4, prc::kParams, 3, prc::kInKeys, prc::kInLabels, 1, kOutOnly, kOutLbl,
    120, sizeof(prc::Prc) + kBlock * sizeof(float), 0, prc::create};

// ---------------------------------------------------------------- VOW
// Vowel filter: three band-passes at the formants of A E I O U (VOW morphs between them), shifted
// by SHF semitones, Q by RES, blended with the dry by MIX.
namespace vow {
enum { kVowel, kShift, kRes, kMix };
const ParamSpec kParams[] = {
    {"vowel", "VOW", 0.f, 4.f, 0.f, Curve::Lin, Unit::None, nullptr},
    {"shift", "SHF", -12.f, 12.f, 0.f, Curve::Lin, Unit::Semi, nullptr},
    {"res", "RES", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"mix", "MIX", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "vowel"};
const char* const kInLabels[] = {"IN", "VOW"};
const float kForm[5][3] = {{730.f, 1090.f, 2440.f}, {530.f, 1840.f, 2480.f}, {270.f, 2290.f, 3010.f},
                           {570.f, 840.f, 2410.f},  {300.f, 870.f, 2240.f}};
const float kGain[3] = {1.f, 0.5f, 0.25f};
}  // namespace vow

void vowFormants(float v, float* f3) {
    v = clampf(v, 0.f, 4.f);
    const int a = (int)v < 4 ? (int)v : 3;
    const float t = v - (float)a;
    for (int k = 0; k < 3; k++) f3[k] = vow::kForm[a][k] + (vow::kForm[a + 1][k] - vow::kForm[a][k]) * t;
}

namespace vow {
class Vow : public Module {
   public:
    Vow() : Module(kVowType) {}
    void silence() override { for (Svf& s : bp_) s.lp = s.bp = 0.f; }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* vc = inOrNull(ins, c, 1);
        const float damp = 0.25f - 0.21f * param(kRes), mix = param(kMix);
        const float shift = fastExp2(param(kShift) * (1.f / 12.f));
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            if ((i & 7) == 0) {  // formants every 8 samples: sinf is slow
                float f[3];
                vowFormants(param(kVowel) + at(vc, i) * 0.8f, f);
                for (int k = 0; k < 3; k++) bp_[k].set(f[k] * shift, damp);
            }
            const float x = in ? in[i] * 0.2f : 0.f;
            float y = 0.f;
            for (int k = 0; k < 3; k++) y += bp_[k].band(x) * damp * kGain[k];  // x damp: unity at each peak (review)
            o[i] = clampf(x * (1.f - mix) + y * mix * 1.4f, -2.f, 2.f) * kAudioVolts;
        }
    }

   private:
    Svf bp_[3];
};
Module* create() { return new Vow(); }
}  // namespace vow

const ModuleTypeInfo kVowType = {
    "vow", "VOW", "Vowel filter: A E I O U formants", Category::Processor, Icon::Vcf, Drawing::Picture,
    4, vow::kParams, 2, vow::kInKeys, vow::kInLabels, 1, kOutOnly, kOutLbl,
    240, sizeof(vow::Vow) + kBlock * sizeof(float), 0, vow::create};

// ---------------------------------------------------------------- EQ3
// 3-band EQ: a low shelf at 200 Hz, a peak at FRQ (Q 0.9), a high shelf at 4 kHz, +-15 dB each.
namespace eq3 {
enum { kLow, kMid, kFreq, kHigh };
const ParamSpec kParams[] = {
    {"low", "LOW", -15.f, 15.f, 0.f, Curve::Lin, Unit::Db, nullptr},
    {"mid", "MID", -15.f, 15.f, 0.f, Curve::Lin, Unit::Db, nullptr},
    {"freq", "FRQ", 200.f, 5000.f, 1000.f, Curve::Log, Unit::Hz, nullptr},
    {"high", "HI", -15.f, 15.f, 0.f, Curve::Lin, Unit::Db, nullptr},
};
class Eq3 : public Module {
   public:
    Eq3() : Module(kEq3Type) {}
    void silence() override { for (Biquad& b : bq_) b.z1 = b.z2 = 0.f; }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float p[4] = {param(kLow), param(kMid), param(kFreq), param(kHigh)};
        if (p[0] != last_[0] || p[1] != last_[1] || p[2] != last_[2] || p[3] != last_[3]) {  // on a change only
            for (int k = 0; k < 4; k++) last_[k] = p[k];
            eq3Design(p[0], p[1], p[2], p[3], bq_);
        }
        float* o = out(0);
        for (int i = 0; i < kBlock; i++) {
            const float x = in ? in[i] : 0.f;
            o[i] = clampf(bq_[2].process(bq_[1].process(bq_[0].process(x))), -40.f, 40.f);
        }
    }

   private:
    Biquad bq_[3];
    float last_[4] = {99.f, 99.f, 99.f, 99.f};
};
Module* create() { return new Eq3(); }
}  // namespace eq3

void eq3Design(float lowDb, float midDb, float midHz, float highDb, Biquad* b3) {
    b3[0].design(Biquad::LowShelf, 200.f, 0.707f, lowDb, kSampleRate);
    b3[1].design(Biquad::Peak, clampf(midHz, 200.f, 5000.f), 0.9f, midDb, kSampleRate);
    b3[2].design(Biquad::HighShelf, 4000.f, 0.707f, highDb, kSampleRate);
}

const ModuleTypeInfo kEq3Type = {
    "eq3", "EQ3", "3-band EQ: shelves + mid peak", Category::Processor, Icon::Vcf, Drawing::Picture,
    4, eq3::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    120, sizeof(eq3::Eq3) + kBlock * sizeof(float), 0, eq3::create};
