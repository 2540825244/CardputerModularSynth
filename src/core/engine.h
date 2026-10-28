#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include "module.h"
#include "patch.h"
#include "program.h"

using HeapFreeFn = uint32_t (*)();
struct Refusal {
    bool ok;
    const char* reason;  // "NO CPU", "NO RAM", "NO DELAY", "MAX 1"
};

constexpr uint32_t kCpuBudget = 4000;  // cycles per sample
constexpr uint32_t kRamReserve = 24 * 1024;

class Engine {
   public:
    explicit Engine(HeapFreeFn heapFree);
    ~Engine();  // audio thread must be stopped
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Refusal canAdd(const Patch&, const ModuleTypeInfo&) const;
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
    uint32_t estimatedCycles() const;

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

    HeapFreeFn heapFree_;
    // UI-thread state
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
