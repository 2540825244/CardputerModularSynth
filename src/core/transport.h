#pragma once
#include <cstdint>

// v1.5: global play / stop. Space in the patch view (with a ROL or EVO in the patch) stops and
// starts every clocked module at once: CLK, ROL, EVO, and the sequencers on ClockIn (SEQ TRG EUC
// TUR). A start bumps the epoch; a module that sees a new epoch goes back to step 1, so they all
// start together. Any thread may call these (atomics); the audio thread reads them once a block.
namespace transport {
bool running();
uint32_t epoch();
void play();  // start (again) from step 1, also when already running
void stop();
void toggle();
}  // namespace transport
