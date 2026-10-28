#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include "module.h"
#include "patch.h"
#include "program.h"

using HeapFreeFn = uint32_t (*)();
using CycleFn = uint32_t (*)();  // free-running cycle counter (device: CCOUNT)
struct Refusal {
    bool ok;
    const char* reason;  // "NO CPU", "NO RAM", "NO BUFFER", "MAX 1"
};

constexpr uint32_t kCpuBudget = 4000;          // cycles per sample the modules may take
constexpr uint32_t kCyclesPerSample = 5000;    // 240 MHz / 48 kHz: what the measured load is relative to
constexpr uint32_t kRamReserve = 24 * 1024;

class Engine {
   public:
    explicit Engine(HeapFreeFn heapFree);
    ~Engine();  // audio thread must be stopped
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // canAdd: against the per-type costs (measured once a type has run, else its estimate).
    // canAddLive: adding to the running patch, so the measured load and the largest free heap
    // block count too.
    Refusal canAdd(const Patch&, const ModuleTypeInfo&) const;
    Refusal canAddLive(const Patch&, const ModuleTypeInfo&) const;
    struct Live {
        uint32_t cycles;   // measured load, cycles per sample
        uint32_t largest;  // largest free heap block (UINT32_MAX when unknown)
    };
    Live live() const;  // read once per frame when checking many types (the heap walk takes a lock)
    Refusal canAddLive(const Patch&, const ModuleTypeInfo&, const Live&) const;
    uint32_t costOf(const ModuleTypeInfo&) const;
    uint32_t measuredCost(const ModuleTypeInfo&) const;  // 0 = not run yet
    void setCycleCounter(CycleFn f) { cycles_ = f; }
    void setLargestBlock(HeapFreeFn f) { largestBlock_ = f; }
    int missing() const { return missing_; }  // modules in the patch with no instance (no RAM)
    bool costsChanged();  // true once after a measured cost rose (device: print them)
    // UI thread. Returns the number of modules that could not be instantiated (resource pool
    // held by a retired module); collect() retries those.
    int apply(const Patch&);
    void setParam(uint16_t id, uint8_t i, float v);
    void setOutAmount(uint16_t id, uint8_t o, float a);
    void setInAmount(uint16_t id, uint8_t i, float a);
    Module* instance(uint16_t id);
    void collect();  // UI thread: frees retired programs/modules the audio thread has passed

    void processBlock(float* mono);  // audio thread: kBlock samples, +-1

    void setCpuLoad(float l) { cpuLoad_.store(l, std::memory_order_relaxed); }
    float cpuLoad() const { return cpuLoad_.load(std::memory_order_relaxed); }
    uint32_t estimatedCycles() const;  // the running patch, from costOf()

   private:
    struct Inst {
        uint16_t id;
        Module* m;
    };
    struct Retired {
        uint32_t gen;
        Module* m;
        Program* p;
    };
    Module* instanceOf(uint16_t id) const;
    void retire(Module* m, Program* p, uint32_t gen);
    static void pushValues(Module*, const ModuleRec&);
    int instantiateAll(const Patch&);  // creates missing, pushes params; returns # missing
    void publish(uint32_t gen);

    struct Learned {
        const ModuleTypeInfo* t;
        uint32_t cycles;  // session max of the instances' smoothed per-sample cost
    };
    void learnCosts();

    HeapFreeFn heapFree_;
    HeapFreeFn largestBlock_ = nullptr;
    CycleFn cycles_ = nullptr;
    // UI-thread state
    std::vector<Learned> learned_;
    bool costsChanged_ = false;
    int missing_ = 0;
    uint32_t collects_ = 0, retryTick_ = 0;
    Patch patch_;
    std::vector<Inst> insts_;
    std::vector<Retired> retired_;
    Program* published_ = nullptr;  // last program handed to the audio thread
    uint32_t gen_ = 0;
    // shared
    std::atomic<Program*> pending_{nullptr};
    std::atomic<uint32_t> seen_{0};
    std::atomic<float> cpuLoad_{0.f};
    // audio-thread state
    Program* cur_ = nullptr;
    float scratch_[kMaxIns][kBlock];
};
