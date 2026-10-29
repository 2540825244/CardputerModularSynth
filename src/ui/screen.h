#pragma once
#include <cstdint>
#include "core/engine.h"
#include "core/patch.h"
#include "ui_state.h"

// Screen sleep (v1.3, owner): no key for the SYSTEM time -> lowest backlight; 3 min later -> off
// and no redraws. Audio keeps running; the first key only wakes (UiController::handle).
enum class Screen : uint8_t { On, Dim, Off };
uint32_t sleepAfterMs(uint8_t setting);  // setting 0..4 = OFF, 1, 2, 5, 10 min; 0 = never
Screen screenFor(uint32_t idleMs, uint8_t setting);

// Everything a frame shows, hashed in two parts. ui: the UI state, the patch and the time-driven
// bits (toast, help, carry blink) - a change draws at once. live: the CPU and RAM bars and the
// activity of the modules in view - they move all the time while audio plays, so they draw at
// most at kLiveMs. ramFrac: heap in use.
struct FrameKeys {
    uint32_t ui, live;
};
FrameKeys frameKeys(const UiState&, const Patch&, Engine&, uint32_t nowMs, bool fnHeld, float ramFrac);

// A ui change draws, then full rate for kSettleMs (short animations - zoom, flashes, easing - run
// on time alone); a live change draws every kLiveMs (not while dim); and once a second regardless.
class RedrawGate {
   public:
    static constexpr uint32_t kSettleMs = 300, kLiveMs = 100, kKeepAliveMs = 1000;
    bool due(const FrameKeys& k, uint32_t nowMs, bool dim = false);

   private:
    bool first_ = true;
    FrameKeys key_{};
    uint32_t changedMs_ = 0, drawnMs_ = 0;
};
