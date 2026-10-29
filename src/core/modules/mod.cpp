#include "mod.h"
#include <cmath>
#include "../scales.h"
#include "Control/adenv.h"
#include "common.h"

using namespace mods;

namespace {
const char* const kInOnly[] = {"in"};
const char* const kInLbl[] = {"IN"};
const char* const kOutOnly[] = {"out"};
const char* const kOutLbl[] = {"OUT"};
const char* const kClkRst[] = {"clk", "rst"};
const char* const kClkRstLbl[] = {"CLK", "RST"};
constexpr float kHigh = kGateHighVolts;

// Clock + reset edges, shared by the sequencers. step() returns true on a clock rising edge.
struct ClockIn {
    GateDetector clk, rst;
    bool high = false, reset = false;
    bool step(const float* c, const float* r, int i) {
        reset = false;
        if (r) {
            rst.process(r[i]);
            reset = rst.rose;
        }
        if (!c) return false;
        clk.process(c[i]);
        high = clk.high;
        return clk.rose;
    }
};
}  // namespace

// ---------------------------------------------------------------- AD
namespace ad {
enum { kAtk, kDec, kCurve, kLoop };
const char* const kOffOn[] = {"OFF", "ON"};
const ParamSpec kParams[] = {
    {"atk", "ATK", 0.5f, 5000.f, 5.f, Curve::Log, Unit::Ms, nullptr},
    {"dec", "DEC", 1.f, 10000.f, 300.f, Curve::Log, Unit::Ms, nullptr},
    {"curve", "CRV", -10.f, 10.f, -2.f, Curve::Lin, Unit::None, nullptr},
    {"loop", "LOP", 0.f, 1.f, 0.f, Curve::Stepped, Unit::Enum, kOffOn},
};
const char* const kInKeys[] = {"trig"};
const char* const kInLabels[] = {"TRG"};
const char* const kOutKeys[] = {"out", "eoc"};
const char* const kOutLabels[] = {"OUT", "EOC"};
class Ad : public Module {
   public:
    Ad() : Module(kAdType) {
        e_.Init(kSampleRate);
        e_.SetMin(0.f);
        e_.SetMax(1.f);
    }
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        e_.SetTime(daisysp::ADENV_SEG_ATTACK, param(kAtk) * 0.001f);
        e_.SetTime(daisysp::ADENV_SEG_DECAY, param(kDec) * 0.001f);
        e_.SetCurve(param(kCurve));
        const bool loop = param(kLoop) >= 0.5f;
        for (int i = 0; i < kBlock; i++) {
            if (tr) {
                g_.process(tr[i]);
                if (g_.rose) e_.Trigger();
            }
            if (loop && !e_.IsRunning()) e_.Trigger();
            const bool was = e_.IsRunning();
            out(0)[i] = e_.Process() * kHigh;
            if (was && !e_.IsRunning()) eoc_ = 48;  // 1 ms end-of-cycle pulse
            out(1)[i] = eoc_ > 0 ? (eoc_--, kHigh) : 0.f;
        }
        setMonitor(0, out(0)[kBlock - 1] / kHigh);
    }

   private:
    daisysp::AdEnv e_;
    GateDetector g_;
    int eoc_ = 0;
};
Module* create() { return new Ad(); }
}  // namespace ad

const ModuleTypeInfo kAdType = {
    "ad", "AD", "AD envelope: curve, loop, EOC", Category::Modulator, Icon::Env, Drawing::Picture,
    4, ad::kParams, 1, ad::kInKeys, ad::kInLabels, 2, ad::kOutKeys, ad::kOutLabels,
    600, sizeof(ad::Ad) + 2 * kBlock * sizeof(float), 0, ad::create};

// ---------------------------------------------------------------- RND
// Random voltages: SMO glides between random targets RATE times a second; STP holds a new one on
// each TRIG (or at RATE when TRIG is unpatched).
namespace rnd {
enum { kRate, kSmooth, kRange };
const ParamSpec kParams[] = {
    {"rate", "RAT", 0.05f, 50.f, 2.f, Curve::Log, Unit::Hz, nullptr},
    {"smooth", "SMO", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"range", "RNG", 0.f, 5.f, 5.f, Curve::Lin, Unit::Volts, nullptr},
};
const char* const kInKeys[] = {"trig"};
const char* const kInLabels[] = {"TRG"};
const char* const kOutKeys[] = {"smo", "stp"};
const char* const kOutLabels[] = {"SMO", "STP"};
class Rnd : public Module {
   public:
    Rnd() : Module(kRndType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* tr = inOrNull(ins, c, 0);
        const float inc = param(kRate) / kSampleRate, range = param(kRange);
        const float k = 1.f - expf(-1.f / ((0.002f + param(kSmooth) / param(kRate)) * kSampleRate));
        for (int i = 0; i < kBlock; i++) {
            bool tick = false;
            if (tr) {
                g_.process(tr[i]);
                tick = g_.rose;
            } else {
                ph_ += inc;
                if (ph_ >= 1.f) {
                    ph_ -= 1.f;
                    tick = true;
                }
            }
            if (tick) target_ = rng_.bi();
            y_ += k * (target_ - y_);
            out(0)[i] = y_ * range;
            out(1)[i] = target_ * range;
        }
    }

   private:
    Rng rng_;
    GateDetector g_;
    float ph_ = 0.f, target_ = 0.f, y_ = 0.f;
};
Module* create() { return new Rnd(); }
}  // namespace rnd

const ModuleTypeInfo kRndType = {
    "rnd", "RND", "Random: smooth and stepped", Category::Modulator, Icon::Dice, Drawing::Picture,
    3, rnd::kParams, 1, rnd::kInKeys, rnd::kInLabels, 2, rnd::kOutKeys, rnd::kOutLabels,
    170, sizeof(rnd::Rnd) + 2 * kBlock * sizeof(float), 0, rnd::create};

// ---------------------------------------------------------------- SEQ
// 12-step pitch sequencer. Page STEP: row 1 = step pitches, row 2 = step gates (bits of GATES),
// row 3 = pins. Page SET: length, gate length, scale. A step with its gate off holds the pitch
// and sends no gate. EOC pulses when the sequence wraps.
namespace seq {
enum { kS0 = 0, kGates = 12, kLen = 13, kGateLen = 14, kScale = 15 };
#define STEP(n) {"s" #n, #n, -24.f, 24.f, 0.f, Curve::Stepped, Unit::Semi, nullptr}
const ParamSpec kParams[] = {
    STEP(1), STEP(2), STEP(3), STEP(4), STEP(5), STEP(6), STEP(7), STEP(8), STEP(9), STEP(10), STEP(11), STEP(12),
    {"gates", "GAT", 0.f, 4095.f, 4095.f, Curve::Stepped, Unit::Int, nullptr},
    {"len", "LEN", 1.f, 12.f, 8.f, Curve::Stepped, Unit::Int, nullptr},
    {"gatelen", "GLN", 0.05f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"scale", "SCL", 0.f, 12.f, 0.f, Curve::Stepped, Unit::Enum, kScaleNames},
};
#undef STEP
const char* const kOutKeys[] = {"pitch", "gate", "eoc"};
const char* const kOutLabels[] = {"PIT", "GAT", "EOC"};
const CellRef kStepCells[36] = {
    cP(0), cP(1), cP(2), cP(3), cP(4), cP(5), cP(6), cP(7), cP(8), cP(9), cP(10), cP(11),
    cB(12, 0), cB(12, 1), cB(12, 2), cB(12, 3), cB(12, 4), cB(12, 5), cB(12, 6), cB(12, 7), cB(12, 8), cB(12, 9), cB(12, 10), cB(12, 11),
    cI(0), cI(1), c_, cO(0), cO(1), cO(2), c_, c_, c_, c_, c_, c_,
};
const CellRef kSetCells[36] = {
    cP(13), cP(14), cP(15), c_, c_, cD, cD, cD, cD, cD, cD, cD,
    c_, c_, c_, c_, c_, cD, cD, cD, cD, cD, cD, cD,
    c_, c_, c_, c_, c_, cD, cD, cD, cD, cD, cD, cD,
};
const PageDef kPages[] = {
    {"STEP", PageKind::Cells, Drawing::Playhead, kStepCells},
    {"SET", PageKind::Cells, Drawing::Picture, kSetCells},
};
class Seq : public Module {
   public:
    Seq() : Module(kSeqType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* ck = inOrNull(ins, c, 0);
        const float* rs = inOrNull(ins, c, 1);
        const int len = (int)param(kLen);
        const uint32_t gates = (uint32_t)param(kGates);
        const int scale = (int)param(kScale);
        for (int i = 0; i < kBlock; i++) {
            const bool rose = clk_.step(ck, rs, i);
            if (clk_.reset) step_ = -1;  // the next clock plays step 1
            if (rose) {
                step_ = (step_ + 1) % (len < 1 ? 1 : len);
                if (step_ == 0) eoc_ = 48;
                period_ = since_ > 0 ? since_ : period_;
                since_ = 0;
                on_ = (gates >> step_) & 1u;
                if (on_) pitch_ = quantize(param(kS0 + step_) / 12.f, scale, 0);
            }
            since_++;
            const bool gate = on_ && (ck ? (float)since_ < param(kGateLen) * (float)period_ : false);
            out(0)[i] = pitch_;
            out(1)[i] = gate ? kHigh : 0.f;
            out(2)[i] = eoc_ > 0 ? (eoc_--, kHigh) : 0.f;
        }
        setMonitor(0, (float)step_);
    }

   private:
    ClockIn clk_;
    int step_ = -1, since_ = 0, period_ = 12000, eoc_ = 0;
    bool on_ = false;
    float pitch_ = 0.f;
};
Module* create() { return new Seq(); }
}  // namespace seq

const ModuleTypeInfo kSeqType = {
    "seq", "SEQ", "12-step pitch sequencer", Category::Modulator, Icon::Seq, Drawing::None,
    16, seq::kParams, 2, kClkRst, kClkRstLbl, 3, seq::kOutKeys, seq::kOutLabels,
    260, sizeof(seq::Seq) + 3 * kBlock * sizeof(float), 0, seq::create, seq::kPages, 2};

// ---------------------------------------------------------------- TRG
// Three trigger lanes x 12 steps, one row each; an output follows the clock while its step is set.
namespace trg {
enum { kL1, kL2, kL3, kLen, kDiv };
const char* const kDivNames[] = {"/1", "/2", "/3", "/4"};
const ParamSpec kParams[] = {
    {"lane1", "L1", 0.f, 4095.f, 0b000100010001, Curve::Stepped, Unit::Int, nullptr},
    {"lane2", "L2", 0.f, 4095.f, 0b000000010000, Curve::Stepped, Unit::Int, nullptr},
    {"lane3", "L3", 0.f, 4095.f, 0b010101010101, Curve::Stepped, Unit::Int, nullptr},
    {"len", "LEN", 1.f, 12.f, 12.f, Curve::Stepped, Unit::Int, nullptr},
    {"div", "DIV", 0.f, 3.f, 0.f, Curve::Stepped, Unit::Enum, kDivNames},
};
const char* const kOutKeys[] = {"t1", "t2", "t3"};
const char* const kOutLabels[] = {"T1", "T2", "T3"};
#define LANE(p) cB(p, 0), cB(p, 1), cB(p, 2), cB(p, 3), cB(p, 4), cB(p, 5), cB(p, 6), cB(p, 7), cB(p, 8), cB(p, 9), cB(p, 10), cB(p, 11)
const CellRef kPattCells[36] = {LANE(0), LANE(1), LANE(2)};
#undef LANE
const CellRef kSetCells[36] = {
    cP(3), cP(4), c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
    cI(0), cI(1), c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
    cO(0), cO(1), cO(2), c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const PageDef kPages[] = {
    {"PATT", PageKind::Cells, Drawing::Playhead, kPattCells},
    {"SET", PageKind::Cells, Drawing::None, kSetCells},
};
class Trg : public Module {
   public:
    Trg() : Module(kTrgType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* ck = inOrNull(ins, c, 0);
        const float* rs = inOrNull(ins, c, 1);
        const int len = (int)param(kLen), div = (int)param(kDiv) + 1;
        for (int i = 0; i < kBlock; i++) {
            const bool rose = clk_.step(ck, rs, i);
            if (clk_.reset) {  // the next clock plays step 1
                step_ = -1;
                count_ = 0;
            }
            if (rose && count_++ % div == 0) step_ = (step_ + 1) % (len < 1 ? 1 : len);
            for (int l = 0; l < 3; l++) {
                const bool on = step_ >= 0 && clk_.high && (((uint32_t)param(kL1 + l) >> step_) & 1u);
                out(l)[i] = on ? kHigh : 0.f;
            }
        }
        setMonitor(0, (float)step_);
    }

   private:
    ClockIn clk_;
    int step_ = -1;
    uint32_t count_ = 0;
};
Module* create() { return new Trg(); }
}  // namespace trg

const ModuleTypeInfo kTrgType = {
    "trg", "TRG", "3-lane x 12-step triggers", Category::Modulator, Icon::Steps, Drawing::None,
    5, trg::kParams, 2, kClkRst, kClkRstLbl, 3, trg::kOutKeys, trg::kOutLabels,
    370, sizeof(trg::Trg) + 3 * kBlock * sizeof(float), 0, trg::create, trg::kPages, 2};

// ---------------------------------------------------------------- EUC
namespace euc {
enum { kSteps, kFills, kRot };
const ParamSpec kParams[] = {
    {"steps", "STP", 1.f, 16.f, 8.f, Curve::Stepped, Unit::Int, nullptr},
    {"fills", "FIL", 0.f, 16.f, 3.f, Curve::Stepped, Unit::Int, nullptr},
    {"rot", "ROT", 0.f, 15.f, 0.f, Curve::Stepped, Unit::Int, nullptr},
};
const char* const kOutKeys[] = {"gate", "inv"};
const char* const kOutLabels[] = {"GAT", "INV"};
}  // namespace euc
bool euclidHit(int steps, int fills, int rot, int i) {
    if (steps < 1) return false;
    fills = fills > steps ? steps : fills;
    return (((i + rot) % steps) * fills) % steps < fills;
}
namespace euc {
class Euc : public Module {
   public:
    Euc() : Module(kEucType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* ck = inOrNull(ins, c, 0);
        const float* rs = inOrNull(ins, c, 1);
        const int steps = (int)param(kSteps), fills = (int)param(kFills), rot = (int)param(kRot);
        for (int i = 0; i < kBlock; i++) {
            if (clk_.step(ck, rs, i)) {
                step_ = (step_ + 1) % (steps < 1 ? 1 : steps);
                hit_ = euclidHit(steps, fills, rot, step_);
            }
            if (clk_.reset) step_ = -1;
            out(0)[i] = clk_.high && hit_ ? kHigh : 0.f;
            out(1)[i] = clk_.high && !hit_ ? kHigh : 0.f;
        }
        setMonitor(0, (float)step_);
    }

   private:
    ClockIn clk_;
    int step_ = -1;
    bool hit_ = false;
};
Module* create() { return new Euc(); }
}  // namespace euc

const ModuleTypeInfo kEucType = {
    "euc", "EUC", "Euclidean rhythm", Category::Modulator, Icon::Euclid, Drawing::Picture,
    3, euc::kParams, 2, kClkRst, kClkRstLbl, 2, euc::kOutKeys, euc::kOutLabels,
    200, sizeof(euc::Euc) + 2 * kBlock * sizeof(float), 0, euc::create};

// ---------------------------------------------------------------- QNT
namespace qnt {
enum { kScale, kRoot, kTrans };
const ParamSpec kParams[] = {
    {"scale", "SCL", 0.f, 12.f, 1.f, Curve::Stepped, Unit::Enum, kScaleNames},
    {"root", "ROT", 0.f, 11.f, 0.f, Curve::Stepped, Unit::Enum, kNoteNames},
    {"trans", "TRN", -12.f, 12.f, 0.f, Curve::Stepped, Unit::Semi, nullptr},
};
const char* const kOutKeys[] = {"out", "trig"};
const char* const kOutLabels[] = {"OUT", "TRG"};
class Qnt : public Module {
   public:
    Qnt() : Module(kQntType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const int scale = (int)param(kScale), root = (int)param(kRoot);
        const float trans = param(kTrans) / 12.f;
        for (int i = 0; i < kBlock; i++) {
            const float q = quantize(at(in, i), scale, root) + trans;
            if (q != last_) {
                last_ = q;
                pulse_ = 48;
            }
            out(0)[i] = q;
            out(1)[i] = pulse_ > 0 ? (pulse_--, kHigh) : 0.f;
        }
    }

   private:
    float last_ = 0.f;
    int pulse_ = 0;
};
Module* create() { return new Qnt(); }
}  // namespace qnt

const ModuleTypeInfo kQntType = {
    "qnt", "QNT", "Quantizer to a scale", Category::Utility, Icon::Quant, Drawing::Picture,
    3, qnt::kParams, 1, kInOnly, kInLbl, 2, qnt::kOutKeys, qnt::kOutLabels,
    630, sizeof(qnt::Qnt) + 2 * kBlock * sizeof(float), 0, qnt::create};

// ---------------------------------------------------------------- TUR
// Turing machine: a LEN-bit loop; on each clock the last bit comes back to the front, flipped with
// probability PROB. CV = the top 8 bits scaled to RANGE volts; GATE = the new front bit (follows the clock).
namespace tur {
enum { kLen, kProb, kRange };
const ParamSpec kParams[] = {
    {"len", "LEN", 2.f, 16.f, 8.f, Curve::Stepped, Unit::Int, nullptr},
    {"prob", "PRB", 0.f, 1.f, 0.1f, Curve::Lin, Unit::Percent, nullptr},
    {"range", "RNG", 0.f, 5.f, 2.f, Curve::Lin, Unit::Volts, nullptr},
};
const char* const kOutKeys[] = {"cv", "gate"};
const char* const kOutLabels[] = {"CV", "GAT"};
class Tur : public Module {
   public:
    Tur() : Module(kTurType) { reg_ = rng_.next() & 0xFFFF; }
    void process(const float* const* ins, const bool* c) override {
        const float* ck = inOrNull(ins, c, 0);
        const float* rs = inOrNull(ins, c, 1);
        const int len = (int)param(kLen);
        for (int i = 0; i < kBlock; i++) {
            if (clk_.step(ck, rs, i)) {
                uint32_t last = (reg_ >> (len - 1)) & 1u;
                if (rng_.uni() < param(kProb)) last ^= 1u;
                reg_ = ((reg_ << 1) | last) & ((1u << len) - 1u);
            }
            const float cv = (float)(reg_ & 0xFF) / 255.f * param(kRange);
            out(0)[i] = cv;
            out(1)[i] = clk_.high && (reg_ & 1u) ? kHigh : 0.f;
        }
        setMonitor(0, (float)reg_);  // the picture's register (<= 16 bits: exact in a float)
    }

   private:
    Rng rng_;
    ClockIn clk_;
    uint32_t reg_ = 0;
};
Module* create() { return new Tur(); }
}  // namespace tur

const ModuleTypeInfo kTurType = {
    "tur", "TUR", "Turing machine: looping random", Category::Modulator, Icon::Dice, Drawing::Picture,
    3, tur::kParams, 2, kClkRst, kClkRstLbl, 2, tur::kOutKeys, tur::kOutLabels,
    260, sizeof(tur::Tur) + 2 * kBlock * sizeof(float), 0, tur::create};

// ---------------------------------------------------------------- LGC
namespace lgc {
const char* const kInKeys[] = {"a", "b"};
const char* const kInLabels[] = {"A", "B"};
const char* const kOutKeys[] = {"and", "or", "xor", "nota"};
const char* const kOutLabels[] = {"AND", "OR", "XOR", "!A"};
class Lgc : public Module {
   public:
    Lgc() : Module(kLgcType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* a = inOrNull(ins, c, 0);
        const float* b = inOrNull(ins, c, 1);
        for (int i = 0; i < kBlock; i++) {
            const bool x = ga_.process(at(a, i)), y = gb_.process(at(b, i));
            out(0)[i] = x && y ? kHigh : 0.f;
            out(1)[i] = x || y ? kHigh : 0.f;
            out(2)[i] = x != y ? kHigh : 0.f;
            out(3)[i] = !x ? kHigh : 0.f;
        }
    }

   private:
    GateDetector ga_, gb_;
};
Module* create() { return new Lgc(); }
}  // namespace lgc

const ModuleTypeInfo kLgcType = {
    "lgc", "LGC", "Logic: AND OR XOR NOT", Category::Utility, Icon::Logic, Drawing::Picture,
    0, nullptr, 2, lgc::kInKeys, lgc::kInLabels, 4, lgc::kOutKeys, lgc::kOutLabels,
    260, sizeof(lgc::Lgc) + 4 * kBlock * sizeof(float), 0, lgc::create};

// ---------------------------------------------------------------- BRN
// Bernoulli gate: each rising IN goes to A with probability PROB, else to B; the chosen output
// follows IN until it falls.
namespace brn {
enum { kProb };
const ParamSpec kParams[] = {
    {"prob", "PRB", 0.f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
};
const char* const kInKeys[] = {"in", "prob"};
const char* const kInLabels[] = {"IN", "PRB"};
const char* const kOutKeys[] = {"a", "b"};
const char* const kOutLabels[] = {"A", "B"};
class Brn : public Module {
   public:
    Brn() : Module(kBrnType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const float* pc = inOrNull(ins, c, 1);
        for (int i = 0; i < kBlock; i++) {
            const bool hi = g_.process(at(in, i));
            if (g_.rose) toA_ = rng_.uni() < clampf(param(kProb) + at(pc, i) * 0.2f, 0.f, 1.f);
            out(0)[i] = hi && toA_ ? kHigh : 0.f;
            out(1)[i] = hi && !toA_ ? kHigh : 0.f;
        }
    }

   private:
    Rng rng_;
    GateDetector g_;
    bool toA_ = true;
};
Module* create() { return new Brn(); }
}  // namespace brn

const ModuleTypeInfo kBrnType = {
    "brn", "BRN", "Bernoulli gate: coin toss", Category::Utility, Icon::Dice, Drawing::Picture,
    1, brn::kParams, 2, brn::kInKeys, brn::kInLabels, 2, brn::kOutKeys, brn::kOutLabels,
    180, sizeof(brn::Brn) + 2 * kBlock * sizeof(float), 0, brn::create};

// ---------------------------------------------------------------- CHS
// Lorenz attractor (sigma 10, rho 28, beta 8/3), Euler steps scaled by RATE; X/Y/Z ~ +-5 V.
namespace chs {
enum { kRate, kRho };
const ParamSpec kParams[] = {
    {"rate", "RAT", 0.01f, 20.f, 0.5f, Curve::Log, Unit::Hz, nullptr},
    {"rho", "RHO", 10.f, 40.f, 28.f, Curve::Lin, Unit::None, nullptr},
};
const char* const kOutKeys[] = {"x", "y", "z"};
const char* const kOutLabels[] = {"X", "Y", "Z"};
class Chs : public Module {
   public:
    Chs() : Module(kChsType) {}
    void process(const float* const* ins, const bool* c) override {
        const float* rc = c[0] ? ins[0] : nullptr;
        const float rho = param(kRho);
        for (int i = 0; i < kBlock; i++) {
            const float dt = clampf(param(kRate) * fastExp2(at(rc, i)) * (0.01f / 48.f), 0.f, 0.005f);
            const float dx = 10.f * (y_ - x_), dy = x_ * (rho - z_) - y_, dz = x_ * y_ - (8.f / 3.f) * z_;
            x_ += dx * dt;
            y_ += dy * dt;
            z_ += dz * dt;
            if (!(fabsf(x_) < 100.f && fabsf(y_) < 100.f && fabsf(z_) < 100.f)) x_ = y_ = z_ = 1.f;
            out(0)[i] = clampf(x_ * 0.25f, -10.f, 10.f);
            out(1)[i] = clampf(y_ * 0.18f, -10.f, 10.f);
            out(2)[i] = clampf((z_ - 25.f) * 0.2f, -10.f, 10.f);
        }
        setMonitor(0, x_);  // the picture's live point (ui/pic_mod.cpp)
        setMonitor(1, z_);
    }

   private:
    float x_ = 1.f, y_ = 1.f, z_ = 1.f;
};
Module* create() { return new Chs(); }
const char* const kInKeys[] = {"rate"};
const char* const kInLabels[] = {"RAT"};
}  // namespace chs

const ModuleTypeInfo kChsType = {
    "chs", "CHS", "Lorenz chaos: X Y Z", Category::Modulator, Icon::Chaos, Drawing::Picture,
    2, chs::kParams, 1, chs::kInKeys, chs::kInLabels, 3, chs::kOutKeys, chs::kOutLabels,
    430, sizeof(chs::Chs) + 3 * kBlock * sizeof(float), 0, chs::create};

// ---------------------------------------------------------------- KNB
// Four knobs as voltages: macro controls to sweep with tilt or to patch into several places.
namespace knb {
const ParamSpec kParams[] = {
    {"k1", "K1", -5.f, 5.f, 0.f, Curve::Lin, Unit::Volts, nullptr},
    {"k2", "K2", -5.f, 5.f, 0.f, Curve::Lin, Unit::Volts, nullptr},
    {"k3", "K3", -5.f, 5.f, 0.f, Curve::Lin, Unit::Volts, nullptr},
    {"k4", "K4", -5.f, 5.f, 0.f, Curve::Lin, Unit::Volts, nullptr},
};
const char* const kOutKeys[] = {"o1", "o2", "o3", "o4"};
const char* const kOutLabels[] = {"O1", "O2", "O3", "O4"};
class Knb : public Module {
   public:
    Knb() : Module(kKnbType) {
        for (Slew& s : s_) s.setTimeMs(5.f, kSampleRate);
    }
    void process(const float* const*, const bool*) override {
        for (int k = 0; k < 4; k++)
            for (int i = 0; i < kBlock; i++) out(k)[i] = s_[k].process(param(k));
    }

   private:
    Slew s_[4];
};
Module* create() { return new Knb(); }
}  // namespace knb

const ModuleTypeInfo kKnbType = {
    "knb", "KNB", "Four knobs as CV", Category::Utility, Icon::Knob, Drawing::Picture,
    4, knb::kParams, 0, nullptr, nullptr, 4, knb::kOutKeys, knb::kOutLabels,
    340, sizeof(knb::Knb) + 4 * kBlock * sizeof(float), 0, knb::create};

// ---------------------------------------------------------------- SCO
// Scope + tuner. Keeps 96 points of IN, one per ZOOM samples, re-armed on a rising zero crossing
// so a steady wave stands still; monitor(0) = the measured frequency (Hz), from zero crossings.
namespace sco {
enum { kZoom, kGain };
const ParamSpec kParams[] = {
    {"zoom", "ZOM", 1.f, 64.f, 4.f, Curve::Log, Unit::Ratio, nullptr},
    {"gain", "GAN", 0.25f, 8.f, 1.f, Curve::Log, Unit::Ratio, nullptr},
};
constexpr int kN = 96;
class Sco : public Module {
   public:
    Sco() : Module(kScoType) {}
    const float* displayBuffer(int* n, int* head) const override {
        *n = kN;
        *head = 0;
        return shown_;
    }
    void process(const float* const* ins, const bool* c) override {
        const float* in = inOrNull(ins, c, 0);
        const int zoom = (int)param(kZoom);
        const float g = param(kGain);
        for (int i = 0; i < kBlock; i++) {
            const float x = at(in, i);
            out(0)[i] = x;  // thru
            since_++;
            if (prev_ < 0.f && x >= 0.f) {  // tuner: average period between rising crossings
                if (since_ > 8 && since_ < 48000) period_ += 0.2f * ((float)since_ - period_);
                since_ = 0;
                if (fill_ < 0) fill_ = 0;  // arm the trace on this edge
            }
            prev_ = x;
            if (fill_ >= 0 && ++sub_ >= zoom) {
                sub_ = 0;
                trace_[fill_++] = x * g;
                if (fill_ >= kN) {
                    for (int k = 0; k < kN; k++) shown_[k] = trace_[k];
                    fill_ = -1;
                }
            }
            if (++idle_ > 24000) {  // no edge for 0.5 s: free-run so DC/LFOs still draw
                idle_ = 0;
                if (fill_ < 0) fill_ = 0;
            }
        }
        if (fill_ == 0) idle_ = 0;
        setMonitor(0, since_ < 48000 && period_ > 0.f ? kSampleRate / period_ : 0.f);
    }

   private:
    float trace_[kN] = {}, shown_[kN] = {};
    int fill_ = 0, sub_ = 0, since_ = 0, idle_ = 0;
    float prev_ = 0.f, period_ = 0.f;
};
Module* create() { return new Sco(); }
}  // namespace sco

const ModuleTypeInfo kScoType = {
    "sco", "SCO", "Scope and tuner", Category::Utility, Icon::Scope, Drawing::Scope,
    2, sco::kParams, 1, kInOnly, kInLbl, 1, kOutOnly, kOutLbl,
    120, sizeof(sco::Sco) + kBlock * sizeof(float), 0, sco::create};

