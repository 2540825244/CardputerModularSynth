#pragma once
#include <atomic>
#include "../dsp_util.h"
#include "../module.h"
#include "../patch.h"

// KIT: plays a drum kit into the PO-33 in one take. 8 trigger lanes x 12 steps (stacked hits:
// several lanes on one step), a step every GAP ms (or each CLK rise), one pass or a loop. The
// engine runs the modules fed only from a lane while that lane's step sounds (kit_groups.h).
extern const ModuleTypeInfo kKitType;

namespace kit {
constexpr int kLanes = 8, kSteps = 12;
enum { kL1, kAccLane = kLanes, kLen, kGap, kAcc, kMode, kNumParams };
constexpr int kAccOut = kLanes;            // out 8: ACC level x 5 V during an accented step
constexpr int kTrigSamples = 240;          // 5 ms triggers
constexpr uint32_t kStepEdge = 1u << 8;    // events(): a step started or the pass ended
}  // namespace kit

class KitModule : public Module {
   public:
    KitModule();
    void start();  // UI thread: a pass starts at the next block (a running one restarts)
    bool running() const { return running_.load(std::memory_order_relaxed); }
    // Audio thread, after process(): the lanes that fired this block (bits 0..7), | kStepEdge
    // when a step began or the pass ended.
    uint32_t events() const { return events_; }
    void process(const float* const* ins, const bool* connected) override;

   private:
    void fire(int step);
    std::atomic<uint32_t> startReq_{0};
    std::atomic<bool> running_{false};
    uint32_t startSeen_ = 0, events_ = 0, lanes_ = 0;
    GateDetector run_, clk_;
    int step_ = -1, toNext_ = 0, trig_ = 0, gap_ = 24000;
    bool acc_ = false;
};

// The CPU of the groups the worst step fires at once: laneCost[l] = the summed cost of lane l's
// modules. A bypassed KIT fires nothing.
uint32_t kitWorstStep(const ModuleRec& kit, const uint32_t* laneCost);
