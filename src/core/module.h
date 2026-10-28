#pragma once
#include <atomic>
#include <cstdint>
#include "param.h"
#include "units.h"

enum class Category : uint8_t { Source, Modulator, Processor, Effect, Utility };  // place-menu page, bar colour
enum class Icon : uint8_t { Keys, Osc, Vcf, Vca, Env, Lfo, Dly, Out, Mix, Atn, Snh, Clk, Mux, Dmx, Tilt, Rev, Fx, Psh, Fold, Drive, Crush, Ring, Noise, Drum, Hat, String, Bell, Seq, Steps, Euclid, Quant, Logic, Dice, Chaos, Knob, Scope, Grind, Slew, Lpg, Fm, Saw };
enum class Drawing : uint8_t { None, FilterCurve, Envelope };  // module-view drawing at cols 5-11, rows 2-4

class Module;

// Module-view cells (rows 1..3). Bit: bit `bit` of param `index`, a step of a sequencer.
enum class CellKind : uint8_t { Empty, Param, In, Out, Drawing, Bit };
struct CellRef {
    CellKind kind;
    uint8_t index;
    uint8_t bit;
};
constexpr CellRef cP(uint8_t i) { return {CellKind::Param, i, 0}; }
constexpr CellRef cI(uint8_t i) { return {CellKind::In, i, 0}; }
constexpr CellRef cO(uint8_t i) { return {CellKind::Out, i, 0}; }
constexpr CellRef cB(uint8_t i, uint8_t bit) { return {CellKind::Bit, i, bit}; }
constexpr CellRef cD = {CellKind::Drawing, 0, 0};
constexpr CellRef c_ = {CellKind::Empty, 0, 0};

// Play: the grid keys go raw to the module's play handler (KEYS); Cells: a normal cell page.
enum class PageKind : uint8_t { Cells, Play };
struct PageDef {
    const char* name;  // <= 5 chars, e.g. "MAIN", "SET"
    PageKind kind;
    Drawing drawing;     // drawn over the Drawing cells
    const CellRef* cells;  // 36 entries: rows 1..3, 12 each; nullptr for a Play page
};

struct ModuleTypeInfo {
    const char* id;     // stable: "keys","osc","vcf","vca","env","lfo","dly","out"
    const char* name;   // 3 letters on tiles
    const char* blurb;  // <= 40 chars for the place-menu details
    Category category;
    Icon icon;
    Drawing drawing;
    uint8_t numParams;
    const ParamSpec* params;
    uint8_t numIns;
    const char* const* inKeys;  // stable lowercase
    const char* const* inLabels;  // <= 3 chars
    uint8_t numOuts;
    const char* const* outKeys;
    const char* const* outLabels;
    uint16_t cpuCycles;     // estimated cycles per sample
    uint32_t ramBytes;      // approx heap use of one instance
    uint8_t maxInstances;   // 0 = unlimited
    Module* (*create)();    // UI thread; nullptr result = resource unavailable
    const PageDef* pages = nullptr;  // nullptr: automatic pages (see src/ui/layout.h)
    uint8_t numPages = 0;
    uint8_t poolSlots = 0;  // big buffers taken from delay_pool (DLY, REV): canAdd refuses past the pool
};

class Module {
   public:
    explicit Module(const ModuleTypeInfo& t);  // allocates t.numOuts x kBlock floats
    virtual ~Module();
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    const ModuleTypeInfo& type() const { return type_; }
    void setParam(int i, float v);  // UI thread: atomic store
    float param(int i) const;       // audio thread: atomic load
    void setOutAmount(int o, float a);
    float outAmount(int o) const;  // default 1.0
    void setInAmount(int i, float a);
    float inAmount(int i) const;   // input attenuverter, default 1.0
    float peak() const { return peak_.load(std::memory_order_relaxed); }  // |out(0)| max, last block
    void setPeak(float p) { peak_.store(p, std::memory_order_relaxed); }  // engine only
    float* out(int o) { return outBuf_ + o * kBlock; }  // kBlock floats, persists between blocks
    float monitor(int i) const;
    void setMonitor(int i, float v);  // 2 atomic slots for UI animation

    // Fills out(o) for kBlock samples. ins[i] is only valid when connected[i].
    virtual void process(const float* const* ins, const bool* connected) = 0;
    virtual const float* masterOut() const { return nullptr; }  // OUT only: +-1 full-scale block

   private:
    const ModuleTypeInfo& type_;
    float* outBuf_;
    std::atomic<float> params_[kMaxParams];
    std::atomic<float> amounts_[kMaxOuts];
    std::atomic<float> inAmounts_[kMaxIns];
    std::atomic<float> peak_{0.f};
    std::atomic<float> monitors_[2];
};
