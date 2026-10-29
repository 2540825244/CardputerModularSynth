#include "out.h"
#include <cmath>

namespace {

enum { kSpk, kLine, kDest, kTrim };

const char* const kDestNames[] = {"SPK", "LINE"};
const ParamSpec kParams[] = {
    {"spk", "SPK", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"line", "LIN", 0.f, 1.f, 0.8f, Curve::Lin, Unit::Percent, nullptr},
    {"dest", "DST", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kDestNames},
    // v1.3: makeup gain on top of either volume, so a quiet patch reaches the PO-33's line in
    {"trim", "TRM", 0.f, 24.f, 0.f, Curve::Lin, Unit::Db, nullptr},
};
const char* const kInKeys[] = {"in"};
const char* const kInLabels[] = {"IN"};

Module* create() { return new OutModule(); }

}  // namespace

OutModule::OutModule() : Module(kOutType) {
    dc_.Init(kSampleRate);
    vol_.setTimeMs(20.f, kSampleRate);
    vol_.y = kParams[kSpk].def;
}

void OutModule::setAutoDest(int d) { autoDest_.store(d, std::memory_order_relaxed); }

void OutModule::process(const float* const* ins, const bool* connected) {
    const int a = autoDest_.load(std::memory_order_relaxed);
    const int dest = a >= 0 ? (a > 0 ? 1 : 0) : (param(kDest) >= 0.5f ? 1 : 0);
    const float trim = param(kTrim);
    if (trim != trimDb_) {  // dB -> gain only when it moves (powf is slow)
        trimDb_ = trim;
        trimGain_ = powf(10.f, trim * 0.05f);
    }
    const float target = param(dest ? kLine : kSpk) * trimGain_;
    const float* in = connected[0] ? ins[0] : nullptr;
    float pk = 0.f;
    for (int i = 0; i < kBlock; i++) {
        const float x = in ? sanitize(in[i]) * (1.f / kAudioVolts) : 0.f;
        const float y = softClip(dc_.Process(x) * vol_.process(target));
        y_[i] = y;
        const float ay = y < 0.f ? -y : y;
        pk = ay > pk ? ay : pk;
    }
    setMonitor(0, pk);
    setMonitor(1, (float)dest);
}

const ModuleTypeInfo kOutType = {
    "out", "OUT", "Master out, speaker/line level", Category::Utility, Icon::Out,
    Drawing::None, 4, kParams, 1, kInKeys, kInLabels, 0, nullptr, nullptr,
    280, sizeof(OutModule), 1, create};
