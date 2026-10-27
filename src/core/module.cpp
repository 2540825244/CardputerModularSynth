#include "module.h"

Module::Module(const ModuleTypeInfo& t) : type_(t) {
    const int n = (t.numOuts > 0 ? t.numOuts : 1) * kBlock;
    outBuf_ = new float[n]();
    for (int i = 0; i < kMaxParams; i++)
        params_[i].store(i < t.numParams ? t.params[i].def : 0.f, std::memory_order_relaxed);
    for (auto& a : amounts_) a.store(1.f, std::memory_order_relaxed);
    for (auto& m : monitors_) m.store(0.f, std::memory_order_relaxed);
}

Module::~Module() { delete[] outBuf_; }

void Module::setParam(int i, float v) {
    if (i >= 0 && i < kMaxParams) params_[i].store(v, std::memory_order_relaxed);
}
float Module::param(int i) const {
    return (i >= 0 && i < kMaxParams) ? params_[i].load(std::memory_order_relaxed) : 0.f;
}
void Module::setOutAmount(int o, float a) {
    if (o >= 0 && o < kMaxOuts) amounts_[o].store(a, std::memory_order_relaxed);
}
float Module::outAmount(int o) const {
    return (o >= 0 && o < kMaxOuts) ? amounts_[o].load(std::memory_order_relaxed) : 0.f;
}
float Module::monitor(int i) const {
    return (i >= 0 && i < 2) ? monitors_[i].load(std::memory_order_relaxed) : 0.f;
}
void Module::setMonitor(int i, float v) {
    if (i >= 0 && i < 2) monitors_[i].store(v, std::memory_order_relaxed);
}
