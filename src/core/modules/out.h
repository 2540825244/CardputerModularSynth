#pragma once
#include <atomic>
#include "../dsp_util.h"
#include "../module.h"
#include "Utility/dcblock.h"

extern const ModuleTypeInfo kOutType;

class OutModule : public Module {
   public:
    OutModule();
    void setAutoDest(int d);  // -1 manual, 0 speaker, 1 line (HP_DET); atomic
    void process(const float* const* ins, const bool* connected) override;
    const float* masterOut() const override { return y_; }

   private:
    std::atomic<int> autoDest_{-1};
    daisysp::DcBlock dc_;
    Slew vol_;
    float y_[kBlock] = {};
};
