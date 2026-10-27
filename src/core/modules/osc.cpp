#include "osc.h"
#include "../dsp_util.h"
#include "Synthesis/oscillator.h"

namespace {

enum { kWave, kOct, kTune, kFine };
enum { kInPitch, kInFm };

const char* const kWaveNames[] = {"SIN", "TRI", "SAW", "SQR"};
const uint8_t kDaisyWave[] = {daisysp::Oscillator::WAVE_SIN, daisysp::Oscillator::WAVE_POLYBLEP_TRI,
                              daisysp::Oscillator::WAVE_POLYBLEP_SAW,
                              daisysp::Oscillator::WAVE_POLYBLEP_SQUARE};

const ParamSpec kParams[] = {
    {"wave", "WAV", 0.f, 3.f, 2.f, Curve::Stepped, Unit::Enum, kWaveNames},
    {"oct", "OCT", -4.f, 4.f, 0.f, Curve::Stepped, Unit::Oct, nullptr},
    {"tune", "TUN", -12.f, 12.f, 0.f, Curve::Lin, Unit::Semi, nullptr},
    {"fine", "FIN", -100.f, 100.f, 0.f, Curve::Lin, Unit::Cents, nullptr},
};
const char* const kInKeys[] = {"pitch", "fm"};
const char* const kInLabels[] = {"PIT", "FM"};
const char* const kOutKeys[] = {"out"};
const char* const kOutLabels[] = {"OUT"};

class Osc : public Module {
   public:
    Osc() : Module(kOscType) {
        osc_.Init(kSampleRate);
        osc_.SetAmp(1.f);  // Init defaults to 0.5
    }

    void process(const float* const* ins, const bool* connected) override {
        const int wi = (int)param(kWave);
        osc_.SetWaveform(kDaisyWave[wi < 0 ? 0 : (wi > 3 ? 3 : wi)]);
        const float base = param(kOct) + param(kTune) * (1.f / 12.f) + param(kFine) * (1.f / 1200.f);
        const float* pit = connected[kInPitch] ? ins[kInPitch] : nullptr;
        const float* fm = connected[kInFm] ? ins[kInFm] : nullptr;
        float* o = out(0);
        float hz = 0.f;
        for (int i = 0; i < kBlock; i++) {
            float v = base;
            if (pit) v += pit[i];
            if (fm) v += fm[i];
            hz = voltsToHz(v);
            hz = hz < 0.1f ? 0.1f : (hz > 20000.f ? 20000.f : hz);
            osc_.SetFreq(hz);
            o[i] = osc_.Process() * kAudioVolts;
        }
        setMonitor(0, hz);
    }

   private:
    daisysp::Oscillator osc_;
};

Module* create() { return new Osc(); }

}  // namespace

const ModuleTypeInfo kOscType = {
    "osc", "OSC", "SINE/TRI/SAW/SQR, 1V/OCT + FM", Category::Source, Icon::Osc, Drawing::None,
    4, kParams, 2, kInKeys, kInLabels, 1, kOutKeys, kOutLabels,
    60, sizeof(Osc) + kBlock * sizeof(float), 0, create};
