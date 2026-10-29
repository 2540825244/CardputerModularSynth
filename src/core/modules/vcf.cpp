#include "vcf.h"
#include <cmath>
#include "../dsp_util.h"
#include "Filters/svf.h"

namespace {

enum { kCutoff, kRes, kMode, kDrive };
enum { kInIn, kInCut, kInRes };
constexpr int kStep = 8;  // SetFreq/SetRes cadence in samples

const char* const kModeNames[] = {"LP", "HP", "BP", "NOT"};
const ParamSpec kParams[] = {
    {"cutoff", "CUT", 20.f, 20000.f, 1200.f, Curve::Log, Unit::Hz, nullptr},
    {"res", "RES", 0.f, 1.f, 0.2f, Curve::Lin, Unit::Percent, nullptr},
    {"mode", "MOD", 0.f, 3.f, 0.f, Curve::Stepped, Unit::Enum, kModeNames},
    {"drive", "DRV", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "cut", "res"};
const char* const kInLabels[] = {"IN", "CUT", "RES"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

class Vcf : public Module {
   public:
    Vcf() : Module(kVcfType) {
        svf_.Init(kSampleRate);
        slew_.setTimeMs(10.f, kSampleRate / (float)kStep);
    }

    void process(const float* const* ins, const bool* connected) override {
        const float cutHz = param(kCutoff);
        if (cutHz != lastCutHz_) {  // log2f only when the knob moved
            lastCutHz_ = cutHz;
            cutVolts_ = log2f(cutHz / kC4Hz);
        }
        if (!seeded_) {  // start at the first target so a loaded patch doesn't sweep
            seeded_ = true;
            slew_.y = cutVolts_;
        }
        const float res0 = param(kRes);
        const int mode = (int)(param(kMode) + 0.5f);
        svf_.SetDrive(param(kDrive));
        const float* in = connected[kInIn] ? ins[kInIn] : nullptr;
        const float* cut = connected[kInCut] ? ins[kInCut] : nullptr;
        const float* rcv = connected[kInRes] ? ins[kInRes] : nullptr;
        float* o = out(0);
        float hz = cutHz, res = res0;
        for (int i = 0; i < kBlock; i += kStep) {
            const float v = slew_.process(cutVolts_) + (cut ? cut[i] : 0.f);
            hz = voltsToHz(v);
            hz = hz < 20.f ? 20.f : (hz > 16000.f ? 16000.f : hz);
            res = res0 + (rcv ? rcv[i] * 0.2f : 0.f);
            res = res < 0.f ? 0.f : (res > 1.f ? 1.f : res);
            if (res != lastRes_) {
                lastRes_ = res;
                svf_.SetRes(res);
            }
            svf_.SetFreq(hz);
            for (int j = i; j < i + kStep; j++) {
                svf_.Process(in ? in[j] * 0.2f : 0.f);
                const float y = mode == 1 ? svf_.High() : (mode == 2 ? svf_.Band() : (mode == 3 ? svf_.Notch() : svf_.Low()));
                o[j] = y * kAudioVolts;
            }
        }
        setMonitor(0, hz);
        setMonitor(1, res);
    }

   private:
    daisysp::Svf svf_;
    Slew slew_;  // cutoff in volts, stepped every kStep samples
    float lastCutHz_ = -1.f, cutVolts_ = 0.f, lastRes_ = -1.f;
    bool seeded_ = false;
};

Module* create() { return new Vcf(); }

}  // namespace

const ModuleTypeInfo kVcfType = {
    "vcf", "VCF", "12 dB LP/HP/BP/notch filter", Category::Processor, Icon::Vcf,
    Drawing::Picture, 4, kParams, 3, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    440, sizeof(Vcf) + kBlock * sizeof(float), 0, create};
