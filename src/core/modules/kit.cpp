#include "kit.h"
#include "common.h"

using namespace mods;
using namespace kit;

namespace {
const char* const kModes[] = {"ONE", "LOOP"};
#define LANE(n, key, lbl) {key, lbl, 0.f, 4095.f, (float)(1 << (n)), Curve::Stepped, Unit::Int, nullptr}
const ParamSpec kParams[] = {
    LANE(0, "l1", "T1"), LANE(1, "l2", "T2"), LANE(2, "l3", "T3"), LANE(3, "l4", "T4"),
    LANE(4, "l5", "T5"), LANE(5, "l6", "T6"), LANE(6, "l7", "T7"), LANE(7, "l8", "T8"),
    {"accl", "ACL", 0.f, 4095.f, 0.f, Curve::Stepped, Unit::Int, nullptr},
    {"len", "LEN", 1.f, 12.f, 8.f, Curve::Stepped, Unit::Int, nullptr},
    {"gap", "GAP", 150.f, 1500.f, 500.f, Curve::Log, Unit::Ms, nullptr},
    {"acc", "ACC", 0.f, 1.f, 1.f, Curve::Lin, Unit::Percent, nullptr},
    {"mode", "MOD", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kModes},
};
#undef LANE
static_assert(sizeof(kParams) / sizeof(kParams[0]) == kNumParams, "KIT params");
const char* const kInKeys[] = {"run", "clk"};
const char* const kInLabels[] = {"RUN", "CLK"};
const char* const kOutKeys[] = {"t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "acc"};
const char* const kOutLabels[] = {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "ACC"};

#define ROW(p) cB(p, 0), cB(p, 1), cB(p, 2), cB(p, 3), cB(p, 4), cB(p, 5), cB(p, 6), cB(p, 7), cB(p, 8), cB(p, 9), cB(p, 10), cB(p, 11)
const CellRef kLanes13[36] = {ROW(0), ROW(1), ROW(2)};
const CellRef kLanes46[36] = {ROW(3), ROW(4), ROW(5)};
const CellRef kLanes78[36] = {ROW(6), ROW(7), ROW(kAccLane)};
#undef ROW
const CellRef kSetCells[36] = {
    cP(kLen), cP(kGap), cP(kAcc), cP(kMode), c_, c_, c_, c_, c_, c_, c_, c_,
    cI(0), cI(1), c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
    cO(0), cO(1), cO(2), cO(3), cO(4), cO(5), cO(6), cO(7), cO(kAccOut), c_, c_, c_,
};
const PageDef kKitPages[] = {
    {"T1-3", PageKind::Cells, Drawing::Playhead, kLanes13},
    {"T4-6", PageKind::Cells, Drawing::Playhead, kLanes46},
    {"T78A", PageKind::Cells, Drawing::Playhead, kLanes78},
    {"SET", PageKind::Cells, Drawing::None, kSetCells},
};
Module* create() { return new KitModule(); }
}  // namespace

KitModule::KitModule() : Module(kKitType) { setMonitor(0, -1.f); }

void KitModule::start() { startReq_.fetch_add(1, std::memory_order_relaxed); }

void KitModule::fire(int step) {
    step_ = step;
    lanes_ = 0;
    for (int l = 0; l < kLanes; l++)
        if (((uint32_t)param(kL1 + l) >> step) & 1u) lanes_ |= 1u << l;
    acc_ = ((uint32_t)param(kAccLane) >> step) & 1u;
    events_ |= kStepEdge | lanes_;
    trig_ = kTrigSamples;
    toNext_ = gap_;
}

void KitModule::process(const float* const* ins, const bool* c) {
    const float* run = inOrNull(ins, c, 0);
    const float* clk = inOrNull(ins, c, 1);
    int len = (int)param(kLen);
    len = len < 1 ? 1 : (len > kSteps ? kSteps : len);
    gap_ = (int)(param(kGap) * (kSampleRate / 1000.f));
    const bool loop = param(kMode) >= 0.5f;
    const float accV = param(kAcc) * kGateHighVolts;
    events_ = 0;
    const uint32_t req = startReq_.load(std::memory_order_relaxed);
    bool start = req != startSeen_;
    startSeen_ = req;
    for (int i = 0; i < kBlock; i++) {
        if (run && run_.process(run[i]) && run_.rose) start = true;
        const bool tick = clk && clk_.process(clk[i]) && clk_.rose;
        if (start) {
            fire(0);
            start = false;
        } else if (step_ >= 0 && (clk ? tick : --toNext_ <= 0)) {
            if (step_ + 1 < len || loop) {
                fire(step_ + 1 < len ? step_ + 1 : 0);
            } else {  // the last step's gap is over: the pass ends
                step_ = -1;
                lanes_ = 0;
                acc_ = false;
                trig_ = 0;
                events_ |= kStepEdge;
            }
        }
        const uint32_t on = trig_ > 0 ? lanes_ : 0;
        for (int l = 0; l < kLanes; l++) out(l)[i] = (on >> l) & 1u ? kGateHighVolts : 0.f;
        out(kAccOut)[i] = acc_ ? accV : 0.f;
        if (trig_ > 0) trig_--;
    }
    running_.store(step_ >= 0, std::memory_order_relaxed);
    setMonitor(0, (float)step_);
}

uint32_t kitWorstStep(const ModuleRec& r, const uint32_t* laneCost) {
    if (r.bypass) return 0;
    int len = (int)r.params[kLen];
    len = len < 1 ? 1 : (len > kSteps ? kSteps : len);
    uint32_t worst = 0;
    for (int s = 0; s < len; s++) {
        uint32_t c = 0;
        for (int l = 0; l < kLanes; l++)
            if (((uint32_t)r.params[kL1 + l] >> s) & 1u) c += laneCost[l];
        worst = c > worst ? c : worst;
    }
    return worst;
}

const ModuleTypeInfo kKitType = {
    "kit", "KIT", "8-lane drum kit pass, on demand", Category::Modulator, Icon::Kit, Drawing::None,
    kNumParams, kParams, 2, kInKeys, kInLabels, 9, kOutKeys, kOutLabels,
    150, sizeof(KitModule) + 9 * kBlock * sizeof(float), 1, create, kKitPages, 4};
