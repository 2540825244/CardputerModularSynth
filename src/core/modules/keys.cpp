#include "keys.h"

namespace {

enum { kOct, kGlide, kVel, kLegato };

const char* const kOffOn[] = {"OFF", "ON"};
const ParamSpec kParams[] = {
    {"oct", "OCT", -3.f, 3.f, 0.f, Curve::Stepped, Unit::Oct, nullptr},
    {"glide", "GLD", 0.f, 1000.f, 0.f, Curve::Sqr, Unit::Ms, nullptr},
    {"vel", "VEL", 0.f, 1.f, 0.8f, Curve::Lin, Unit::Percent, nullptr},
    {"legato", "LEG", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kOffOn},
};
const char* const kOutKeys[] = {"pitch", "gate", "vel"};
const char* const kOutLabels[] = {"PIT", "GAT", "VEL"};

Module* create() { return new KeysModule(); }

}  // namespace

KeysModule::KeysModule() : Module(kKeysType) {}

void KeysModule::noteOn(int midiNote) {
    note_.store(midiNote, std::memory_order_relaxed);
    gate_.store(true, std::memory_order_relaxed);
    trigCount_.fetch_add(1, std::memory_order_release);
}

void KeysModule::gateOff() { gate_.store(false, std::memory_order_release); }

void KeysModule::process(const float* const*, const bool*) {
    const uint32_t trig = trigCount_.load(std::memory_order_acquire);
    const bool gate = gate_.load(std::memory_order_relaxed);
    const int note = note_.load(std::memory_order_relaxed);
    const bool legato = param(kLegato) >= 0.5f;
    if (trig != lastTrig_) {
        lastTrig_ = trig;
        // New trigger on a high gate: drop it for this whole block so ENV retriggers.
        if (!legato && prevGateHigh_) gapPending_ = true;
        // Released before this block: still give the gate one full block so the note isn't lost.
        if (!gate) minHold_ = true;
    }
    const float glide = param(kGlide);
    if (glide != lastGlide_) {
        lastGlide_ = glide;
        glideSlew_.setTimeMs(glide, kSampleRate);
    }
    const float target = (float)(note - 60) * (1.f / 12.f) + param(kOct);
    const bool high = (gate || minHold_) && !gapPending_;
    gapPending_ = false;
    if (high) minHold_ = false;
    const float vel = param(kVel) * kGateHighVolts;
    float* p = out(0);
    float* g = out(1);
    float* v = out(2);
    for (int i = 0; i < kBlock; i++) {
        p[i] = glideSlew_.process(target);
        g[i] = high ? kGateHighVolts : 0.f;
        v[i] = vel;
    }
    prevGateHigh_ = high;
    setMonitor(0, (float)note);
    setMonitor(1, gate ? 1.f : 0.f);
}

const ModuleTypeInfo kKeysType = {
    "keys", "KEY", "Keyboard: pitch, gate, velocity", Category::Source, Icon::Keys, Drawing::None,
    4, kParams, 0, nullptr, nullptr, 3, kOutKeys, kOutLabels,
    10, sizeof(KeysModule) + 3 * kBlock * sizeof(float), 1, create};
