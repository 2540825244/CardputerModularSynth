#define TEST  // stmlib's portable code paths (lib/plaits/library.json builds with the same flag)
#include "grd.h"
#include <new>
#include "common.h"
#include "plaits/dsp/voice.h"

using namespace mods;

namespace {

enum { kEng, kOct, kTune, kHarm, kTimb, kMorph, kFmAmt, kTAmt, kMAmt, kDecay, kColor };
enum { kInPitch, kInTrig, kInFm, kInHarm, kInTimb, kInMorph, kInLevel, kInEng };

// ENG 0..15 = the original Plaits/Grind engines (Plaits indices 8..23), 16..23 = the v1.2 ones (0..7).
const char* const kEngNames[] = {"VA", "WSH", "FM", "GRN", "ADD", "WTB", "CHD", "SPC", "SWM", "NOI", "PRT", "STR",
                                 "MDL", "BD", "SD", "HH", "VAF", "PHD", "6OA", "6OB", "6OC", "WTR", "STM", "CHP"};
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
};
const char* const kInKeys[] = {"pitch", "trig", "fm", "harm", "timb", "morph", "level", "eng"};
const char* const kInLabels[] = {"PIT", "TRG", "FM", "HAR", "TIM", "MOR", "LVL", "ENG"};
const char* const kOutKeys[] = {"out", "aux"};
const char* const kOutLabels[] = {"OUT", "AUX"};

const CellRef kMainCells[36] = {
    cP(0), cP(1), cP(2), cP(3), cP(4), cP(5), c_, c_, c_, c_, c_, c_,
    cI(0), cI(1), cI(2), cI(3), cI(4), cI(5), cI(6), cI(7), c_, c_, c_, c_,
    cO(0), cO(1), c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const CellRef kModCells[36] = {
    cP(6), cP(7), cP(8), cP(9), cP(10), c_, c_, c_, c_, c_, c_, c_,
    c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
    c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const PageDef kGrdPages[] = {
    {"MAIN", PageKind::Cells, Drawing::None, kMainCells},
    {"MOD", PageKind::Cells, Drawing::None, kModCells},
};

constexpr size_t kShared = 16384;  // Plaits' engine buffer
constexpr int kSub = 16;           // render 32 samples as 2 x 16 (Plaits max is 24)

class Grd : public Module {
   public:
    explicit Grd(char* shared) : Module(kGrdType), shared_(shared) {
        stmlib::BufferAllocator a(shared_, kShared);
        voice_.Init(&a);
    }
    ~Grd() override { delete[] shared_; }

    void process(const float* const* ins, const bool* c) override {
        plaits::Patch p = {};
        p.note = 60.f + param(kOct) * 12.f + param(kTune);
        p.harmonics = param(kHarm);
        p.timbre = param(kTimb);
        p.morph = param(kMorph);
        p.frequency_modulation_amount = param(kFmAmt);
        p.timbre_modulation_amount = param(kTAmt);
        p.morph_modulation_amount = param(kMAmt);
        p.engine = grdEngineIndex((int)param(kEng));
        p.decay = param(kDecay);
        p.lpg_colour = param(kColor);
        float* o = out(0);
        float* x = out(1);
        for (int s = 0; s < kBlock; s += kSub) {
            plaits::Modulations m = {};
            // CVs: pitch 1 V/oct; 5 V = the full knob range elsewhere (attenuate with the input amounts).
            auto cv = [&](int in) { return c[in] ? ins[in][s] * 0.2f : 0.f; };
            m.note = c[kInPitch] ? ins[kInPitch][s] * 12.f : 0.f;
            m.frequency = c[kInFm] ? ins[kInFm][s] * 12.f : 0.f;
            m.harmonics = cv(kInHarm);
            m.timbre = cv(kInTimb);
            m.morph = cv(kInMorph);
            m.engine = cv(kInEng);
            m.level = cv(kInLevel);
            m.frequency_patched = c[kInFm];
            m.timbre_patched = c[kInTimb];
            m.morph_patched = c[kInMorph];
            m.trigger_patched = c[kInTrig];
            m.level_patched = c[kInLevel];
            float trig = 0.f;  // a trigger anywhere in the sub-block counts
            if (c[kInTrig])
                for (int i = s; i < s + kSub; i++) trig = ins[kInTrig][i] > trig ? ins[kInTrig][i] : trig;
            m.trigger = trig * 0.2f;
            plaits::Voice::Frame f[kSub];
            voice_.Render(p, m, f, kSub);
            for (int i = 0; i < kSub; i++) {
                o[s + i] = (float)f[i].out * (kAudioVolts / 32768.f);
                x[s + i] = (float)f[i].aux * (kAudioVolts / 32768.f);
            }
        }
        setMonitor(0, (float)voice_.active_engine());
    }

   private:
    char* shared_;
    plaits::Voice voice_;
};

Module* create() {
    char* shared = new (std::nothrow) char[kShared];
    if (!shared) return nullptr;
    Grd* g = new (std::nothrow) Grd(shared);
    if (!g) delete[] shared;
    return g;
}

}  // namespace

int grdEngineIndex(int v) {
    v = v < 0 ? 0 : (v > 23 ? 23 : v);
    return v < 16 ? v + 8 : v - 16;
}

const ModuleTypeInfo kGrdType = {
    "grd", "GRD", "Macro osc: 24 Plaits engines", Category::Source, Icon::Grind, Drawing::None,
    11, kParams, 8, kInKeys, kInLabels, 2, kOutKeys, kOutLabels,
    2000, sizeof(Grd) + kShared + 2 * kBlock * sizeof(float), 1, create, kGrdPages, 2};
