#include "env.h"
#include "../dsp_util.h"
#include "Control/adsr.h"

namespace {

enum { kA, kD, kS, kR };
enum { kInGate, kInRetrig };

const ParamSpec kParams[] = {
    {"a", "ATK", 0.5f, 5000.f, 5.f, Curve::Log, Unit::Ms, nullptr},
    {"d", "DEC", 1.f, 5000.f, 200.f, Curve::Log, Unit::Ms, nullptr},
    {"s", "SUS", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"r", "REL", 1.f, 10000.f, 300.f, Curve::Log, Unit::Ms, nullptr},
};
const char* const kInKeys[] = {"gate", "retrig"};
const char* const kInLabels[] = {"GAT", "RTG"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

class Env : public Module {
   public:
    Env() : Module(kEnvType) { adsr_.Init(kSampleRate); }

    void process(const float* const* ins, const bool* connected) override {
        // Adsr skips recomputing when a time is unchanged, so this is cheap.
        adsr_.SetAttackTime(param(kA) * 0.001f);
        adsr_.SetDecayTime(param(kD) * 0.001f);
        adsr_.SetReleaseTime(param(kR) * 0.001f);
        const float sus = param(kS);
        adsr_.SetSustainLevel(sus);
        const float* g = connected[kInGate] ? ins[kInGate] : nullptr;
        const float* rt = connected[kInRetrig] ? ins[kInRetrig] : nullptr;
        float* o = out(0);
        float lvl = 0.f;
        for (int i = 0; i < kBlock; i++) {
            const bool gate = gate_.process(g ? g[i] : 0.f);
            if (rt) {
                retrig_.process(rt[i]);
                // Retrigger only restarts the attack; with the gate low nothing would release it.
                if (retrig_.rose && gate) adsr_.Retrigger(false);
            }
            lvl = adsr_.Process(gate);  // a gate rising edge restarts the attack itself
            o[i] = lvl * kAudioVolts;
        }
        // Adsr has no sustain stage: DECAY that has reached the sustain level is "S".
        float stage = (float)adsr_.GetCurrentSegment();
        if (stage == 2.f && lvl <= sus + 0.002f) stage = 3.f;
        setMonitor(0, stage);
        setMonitor(1, lvl);
    }

   private:
    daisysp::Adsr adsr_;
    GateDetector gate_, retrig_;
};

Module* create() { return new Env(); }

}  // namespace

const ModuleTypeInfo kEnvType = {
    "env", "ENV", "ADSR 0-5V, gate + retrigger", Category::Modulator, Icon::Env,
    Drawing::Envelope, 4, kParams, 2, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    25, sizeof(Env) + kBlock * sizeof(float), 0, create};
