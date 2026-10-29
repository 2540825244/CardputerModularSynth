#define TEST  // stmlib's portable code paths (lib/plaits/library.json builds with the same flag)
#include "../modsynth_iram.h"
#include "grd.h"
#include <new>
#include "common.h"
#include "Control/adsr.h"
#include "../ladder.h"
#include "plaits/dsp/voice.h"

using namespace mods;

namespace {

enum { kEng, kOct, kTune, kHarm, kTimb, kMorph, kFmAmt, kTAmt, kMAmt, kDecay, kColor,
       kVca, kFlt, kCut, kRes, kFeg, kAtk, kDcy, kSus, kLRate, kLCut, kLTim };
enum { kInPitch, kInTrig, kInFm, kInHarm, kInTimb, kInMorph, kInLevel, kInEng, kInCut };
enum { kVcaEnv, kVcaLpg, kVcaDrone };
enum { kFltLp, kFltHp, kFltOff };

// ENG 0..15 = the original Plaits/Grind engines (Plaits indices 8..23), 16..23 = the v1.2 ones (0..7).
const char* const kEngNames[] = {"VA", "WSH", "FM", "GRN", "ADD", "WTB", "CHD", "SPC", "SWM", "NOI", "PRT", "STR",
                                 "MDL", "BD", "SD", "HH", "VAF", "PHD", "6OA", "6OB", "6OC", "WTR", "STM", "CHP"};
const char* const kVcaNames[] = {"ENV", "LPG", "DRN"};
const char* const kFltNames[] = {"LP", "HP", "OFF"};
// 0..10 are the v1.2 Plaits voice; 11.. make it a whole Grind voice: osc -> ladder -> VCA, an
// ADS envelope (release = decay, as on the Grind) and a triangle LFO.
const ParamSpec kParams[] = {
    {"eng", "ENG", 0.f, 23.f, 0.f, Curve::Stepped, Unit::Enum, kEngNames},
    {"oct", "OCT", -4.f, 4.f, 0.f, Curve::Stepped, Unit::Oct, nullptr},
    {"tune", "TUN", -12.f, 12.f, 0.f, Curve::Lin, Unit::Semi, nullptr},
    {"harm", "HAR", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"timb", "TIM", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"morph", "MOR", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"fmamt", "FMA", -1.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
    {"tamt", "TMA", -1.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
    {"mamt", "MMA", -1.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
    {"decay", "DEC", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"color", "COL", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"vca", "VCA", 0.f, 2.f, 0.f, Curve::Stepped, Unit::Enum, kVcaNames},
    {"flt", "FLT", 0.f, 2.f, 0.f, Curve::Stepped, Unit::Enum, kFltNames},
    {"cut", "CUT", 20.f, 18000.f, 2400.f, Curve::Log, Unit::Hz, nullptr},
    {"res", "RES", 0.f, 1.f, 0.2f, Curve::Lin, Unit::Percent, nullptr},
    {"feg", "FEG", -1.f, 1.f, 0.3f, Curve::Lin, Unit::Percent, nullptr},
    {"atk", "ATK", 0.5f, 5000.f, 3.f, Curve::Log, Unit::Ms, nullptr},
    {"dcy", "DCY", 1.f, 10000.f, 400.f, Curve::Log, Unit::Ms, nullptr},
    {"sus", "SUS", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"lrate", "LFO", 0.01f, 40.f, 2.f, Curve::Log, Unit::Hz, nullptr},
    {"lcut", "LCT", -1.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
    {"ltim", "LTM", -1.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"pitch", "trig", "fm", "harm", "timb", "morph", "level", "eng", "cut"};
const char* const kInLabels[] = {"PIT", "TRG", "FM", "HAR", "TIM", "MOR", "LVL", "ENG", "CUT"};
const char* const kOutKeys[] = {"out", "aux", "env", "lfo"};
const char* const kOutLabels[] = {"OUT", "AUX", "ENV", "LFO"};

const CellRef kMainCells[36] = {
    cP(0), cP(1), cP(2), cP(3), cP(4), cP(5), c_, c_, c_, c_, c_, c_,
    cI(0), cI(1), cI(2), cI(3), cI(4), cI(5), cI(6), cI(7), cI(8), c_, c_, c_,
    cO(0), cO(1), cO(2), cO(3), c_, c_, c_, c_, c_, c_, c_, c_,
};
const CellRef kVoiceCells[36] = {
    cP(12), cP(13), cP(14), cP(15), c_, c_, c_, c_, c_, c_, c_, c_,
    cP(11), cP(16), cP(17), cP(18), c_, c_, c_, c_, c_, c_, c_, c_,
    cP(19), cP(20), cP(21), c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const CellRef kModCells[36] = {
    cP(6), cP(7), cP(8), cP(9), cP(10), c_, c_, c_, c_, c_, c_, c_,
    c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
    c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const PageDef kGrdPages[] = {
    {"MAIN", PageKind::Cells, Drawing::None, kMainCells},
    {"VOICE", PageKind::Cells, Drawing::None, kVoiceCells},
    {"MOD", PageKind::Cells, Drawing::None, kModCells},
};

constexpr size_t kShared = 16384;            // Plaits' engine buffer
constexpr int kChunk = (int)plaits::kBlockSize;  // Plaits' envelopes and LPG step once per 12 samples

// Two heap blocks, the 16 KB Plaits buffer and the ~11 KB object: on a fragmented heap two
// blocks are found far more often than one of ~28 KB. canAddLive checks the bigger one; if the
// second then fails, create() returns nullptr and the UI toasts "NO RAM".
class Grd : public Module {
   public:
    explicit Grd(char* shared) : Module(kGrdType), shared_(shared) {
        stmlib::BufferAllocator a(shared_, kShared);
        voice_.Init(&a);
        lad_.Init(kSampleRate);
        env_.Init(kSampleRate);
        cutSlew_.setTimeMs(10.f, kSampleRate / 8.f);
    }
    ~Grd() override { delete[] shared_; }
    // Its KIT group stopped mid-hit: the next hit starts as the first did.
    void silence() override {
        stmlib::BufferAllocator a(shared_, kShared);
        voice_.Silence(&a);
        lad_.Init(kSampleRate);
        env_.Init(kSampleRate);
        gate_ = GateDetector();
        pos_ = kChunk;
        trigMax_ = 0.f;
    }

    MODSYNTH_IRAM void process(const float* const* ins, const bool* c) override {
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
        lad_.SetFilterMode(flt == kFltHp ? Ladder::HP24 : Ladder::LP24);
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
        // Pass 1 per sample: LFO, triggers, renders, envelope, cutoff (every 8), raw voice, gain.
        // Pass 2: the ladder over 8-sample runs. Pass 3: the VCA. Same arithmetic as one pass.
        float v[kBlock], g[kBlock], cut[kBlock / 8];
        const float* cutIn = inOrNull(ins, c, kInCut);
        const float* lvl = vca != kVcaLpg && c[kInLevel] ? ins[kInLevel] : nullptr;
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
                const float oct = cutSlew_.process(base) + feg * e + lCut * lfo + at(cutIn, i);
                hz = hzClamp(voltsToHz(oct), 20.f, 18000.f);
                cut[i >> 3] = hz;
            }
            const plaits::Voice::Frame& f = fifo_[pos_++];
            v[i] = (float)f.out * (1.f / 32768.f);
            float gi = vca == kVcaEnv && gated ? e : 1.f;
            if (lvl) gi *= clampf(lvl[i] * 0.2f, 0.f, 1.f);
            g[i] = gi;
            x[i] = (float)f.aux * (kAudioVolts / 32768.f);
            eo[i] = e * kAudioVolts;
            lo[i] = lfo * kAudioVolts;
        }
        for (int r = 0; r < kBlock / 8; r++) {
            lad_.SetFreq(cut[r]);  // even with the filter off, as before (keeps the coefficients warm)
            if (flt != kFltOff) lad_.ProcessBlock(v + r * 8, 8);
        }
        for (int i = 0; i < kBlock; i++) o[i] = v[i] * g[i] * kAudioVolts;
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
    Ladder lad_;
    daisysp::Adsr env_;
    GateDetector gate_;
    Slew cutSlew_;
    float lfoPh_ = 0.f;
    bool seeded_ = false;
};

Module* create() {
    char* shared = new (std::nothrow) char[kShared];
    if (!shared) return nullptr;
    Grd* g = new (std::nothrow) Grd(shared);
    if (!g) delete[] shared;
    return g;
}

constexpr uint32_t kLargest = sizeof(Grd) > kShared ? sizeof(Grd) : kShared;

}  // namespace

int grdEngineIndex(int v) {
    v = v < 0 ? 0 : (v > 23 ? 23 : v);
    return v < 16 ? v + 8 : v - 16;
}

const ModuleTypeInfo kGrdType = {
    "grd", "GRD", "Grind voice: Plaits+ladder+ADS", Category::Source, Icon::Grind, Drawing::None,
    22, kParams, 9, kInKeys, kInLabels, 4, kOutKeys, kOutLabels,
    // cpuCycles: the worst engine with the ladder on, measured on the device (v1.3 Task 4: bench
    // at UI 30 fps 2234, real firmware 2240, both ENG 11 STR); the engine can change live, so
    // it is the worst engine's, whatever ENG is.
    2240, sizeof(Grd) + kShared + 4 * kBlock * sizeof(float), 1, create, kGrdPages, 3, 0, kLargest, true};
