#pragma once
#include <atomic>
#include <cstdint>
#include "param.h"
#include "units.h"

enum class Category : uint8_t { Source, Modulator, Processor };  // bar colour; IO counts as Processor
enum class Icon : uint8_t { Keys, Osc, Vcf, Vca, Env, Lfo, Dly, Out, Mix, Atn, Snh, Clk, Mux, Dmx, Tilt };
enum class Drawing : uint8_t { None, FilterCurve, Envelope };  // module-view drawing at cols 5-11, rows 2-4

class Module;

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
    std::atomic<float> monitors_[2];
};
