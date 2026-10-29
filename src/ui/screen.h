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

// Everything a frame shows, hashed: the UI state, the patch, the CPU and RAM bars, the activity of
// the modules in view, and the time-driven bits (toast, help, carry blink). ramFrac: heap in use.
uint32_t frameKey(const UiState&, const Patch&, Engine&, uint32_t nowMs, bool fnHeld, float ramFrac);

// Redraw only when the frame would change: then at full rate for kSettleMs (short animations - zoom,
// flashes, easing - run on time alone), and once a second regardless.
class RedrawGate {
   public:
    static constexpr uint32_t kSettleMs = 300, kKeepAliveMs = 1000;
    bool due(uint32_t key, uint32_t nowMs);

   private:
    bool first_ = true;
    uint32_t key_ = 0, changedMs_ = 0, drawnMs_ = 0;
};
