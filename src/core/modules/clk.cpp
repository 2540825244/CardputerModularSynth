#include "clk.h"
#include "../dsp_util.h"

namespace {

enum { kBpm, kDiv };
enum { kInClk, kInRst };

const char* const kDivNames[] = {"/1", "/2", "/3", "/4", "/6", "/8", "/16"};
const int kDivs[] = {1, 2, 3, 4, 6, 8, 16};
constexpr uint32_t kCountWrap = 48;  // lcm of kDivs

const ParamSpec kParams[] = {
    {"bpm", "BPM", 20.f, 300.f, 120.f, Curve::Log, Unit::Bpm, nullptr},
    {"div", "DIV", 0.f, 6.f, 1.f, Curve::Stepped, Unit::Enum, kDivNames},
};
const char* const kInKeys[] = {"clk", "rst"};
const char* const kInLabels[] = {"CLK", "RST"};
const char* const kOutKeys[] = {"out", "div"};
const char* const kOutLabels[] = {"OUT", "DIV"};

// One pulse per beat (high for the first half). A patched CLK replaces the internal clock and
// OUT mirrors it. DIV goes high on every Nth base pulse and falls with it. RST restarts both.
class Clk : public Module {
   public:
    Clk() : Module(kClkType) {}

    void process(const float* const* ins, const bool* connected) override {
        const float bpm = param(kBpm);
        int d = (int)(param(kDiv) + 0.5f);
        d = d < 0 ? 0 : (d > 6 ? 6 : d);
        const uint32_t n = (uint32_t)kDivs[d];
        // 32-bit phase: one wrap per beat, so the period has no float drift.
        const uint32_t inc = (uint32_t)(bpm / 60.f / kSampleRate * 4294967296.f);
        const float* clk = connected[kInClk] ? ins[kInClk] : nullptr;
        const float* rst = connected[kInRst] ? ins[kInRst] : nullptr;
        float* o = out(0);
        float* od = out(1);
        for (int i = 0; i < kBlock; i++) {
            bool high;
            if (clk) {
                high = extGate_.process(clk[i]);
            } else {
                high = phase_ < 0x80000000u;
                phase_ += inc;
            }
            if (rst) {
                rstGate_.process(rst[i]);
                if (rstGate_.rose) {
                    count_ = 0;
                    prevHigh_ = false;
                    if (!clk) {
                        phase_ = inc;
                        high = true;
                    }
                }
            }
            if (high && !prevHigh_) {
                divHigh_ = count_ % n == 0;
                count_ = (count_ + 1) % kCountWrap;
            }
            if (!high) divHigh_ = false;
            prevHigh_ = high;
            o[i] = high ? kGateHighVolts : 0.f;
            od[i] = divHigh_ ? kGateHighVolts : 0.f;
        }
        setMonitor(0, prevHigh_ ? 1.f : 0.f);
    }

   private:
    GateDetector extGate_, rstGate_;
    uint32_t phase_ = 0, count_ = 0;
    bool prevHigh_ = false, divHigh_ = false;
};

Module* create() { return new Clk(); }

}  // namespace

const ModuleTypeInfo kClkType = {
    "clk", "CLK", "Clock 20-300 BPM + divider", Category::Modulator, Icon::Clk, Drawing::None,
    2, kParams, 2, kInKeys, kInLabels, 2, kOutKeys, kOutLabels,
    12, sizeof(Clk) + 2 * kBlock * sizeof(float), 0, create};
