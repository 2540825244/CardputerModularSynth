#pragma once
#include <atomic>
#include "../dsp_util.h"
#include "../module.h"

extern const ModuleTypeInfo kKeysType;

class KeysModule : public Module {
   public:
    KeysModule();
    void noteOn(int midiNote);  // UI thread: sets note, gate high, bumps trigger counter
    void gateOff();             // UI thread: gate low
    void process(const float* const* ins, const bool* connected) override;

   private:
    std::atomic<int> note_{60};
    std::atomic<bool> gate_{false};
    std::atomic<uint32_t> trigCount_{0};
    uint32_t lastTrig_ = 0;
    bool prevGateHigh_ = false, gapPending_ = false, minHold_ = false;
    Slew glideSlew_;
    float lastGlide_ = -1.f;
};
