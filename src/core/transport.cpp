#include "transport.h"
#include <atomic>

namespace transport {
namespace {
std::atomic<bool> gRunning{true};
std::atomic<uint32_t> gEpoch{0};
}  // namespace

bool running() { return gRunning.load(std::memory_order_relaxed); }
uint32_t epoch() { return gEpoch.load(std::memory_order_acquire); }
void play() {
    gEpoch.fetch_add(1, std::memory_order_release);
    gRunning.store(true, std::memory_order_relaxed);
}
void stop() { gRunning.store(false, std::memory_order_relaxed); }
void toggle() {
    if (running()) stop();
    else play();
}
}  // namespace transport
