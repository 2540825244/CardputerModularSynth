#include "screen.h"
#include "side_icons.h"
#include "core/transport.h"

uint32_t sleepAfterMs(uint8_t setting) {
    static const uint32_t kMin[] = {0, 1, 2, 5, 10};
    return kMin[setting < 5 ? setting : 2] * 60000u;
}

Screen screenFor(uint32_t idleMs, uint8_t setting) {
    const uint32_t dim = sleepAfterMs(setting);
    if (!dim || idleMs < dim) return Screen::On;
    return idleMs < dim + 3 * 60000u ? Screen::Dim : Screen::Off;
}

namespace {
struct Fnv {
    uint32_t h = 2166136261u;
    void bytes(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    }
    template <class T>
    void add(const T& v) { bytes(&v, sizeof v); }
};
int px(float f) { return (int)((f < 0.f ? 0.f : (f > 1.f ? 1.f : f)) * 135.f + 0.5f); }  // bar pixels
int q(float v, float steps) { return (int)(v * steps); }
}  // namespace

FrameKeys frameKeys(const UiState& s, const Patch& p, Engine& e, uint32_t now, bool fnHeld, float ramFrac) {
    Fnv f, l;
    f.add(s);  // value-initialised and only ever assigned: its padding stays put
    for (int i = 0; i < p.moduleCount(); i++) f.add(p.moduleAt(i));
    for (int i = 0; i < p.cableCount(); i++) f.add(p.cableAt(i));
    f.add(fnHeld);
    f.add(transport::running());
    f.add((bool)(s.toast[0] && (int32_t)(now - s.toastUntilMs) < 0));
    f.add(helpVisible(s, now));
    for (int i = 0; i < 8; i++) f.add(sidePressed(s, i, now));
    if (s.carry.active || s.sideHeld) f.add((now / 250) % 2);  // carry pulses, held lit keys blink
    l.add(px(e.cpuLoad()));
    l.add(px(ramFrac));
    // Live readouts of the modules in view: tile meters on this page, or the open module's view.
    for (int i = 0; i < p.moduleCount(); i++) {
        const ModuleRec& r = p.moduleAt(i);
        const bool shown = s.view == View::Patch ? r.page == s.page : (s.view == View::Module && r.id == s.openId);
        if (!shown) continue;
        const Module* m = e.instance(r.id);
        if (!m) continue;
        l.add(q(m->peak(), 32.f / kAudioVolts));
        l.add(q(m->monitor(0), 64.f));
        l.add(q(m->monitor(1), 64.f));
        int n = 0, head = 0;
        if (s.view == View::Module && m->displayBuffer(&n, &head)) l.add(head);  // a scope scrolls
    }
    return {f.h, l.h};
}

bool RedrawGate::due(const FrameKeys& k, uint32_t now, bool dim) {
    const bool ui = first_ || k.ui != key_.ui, live = k.live != key_.live;
    first_ = false;
    if (ui) changedMs_ = now;
    const bool draw = ui || now - changedMs_ < kSettleMs || (live && !dim && now - drawnMs_ >= kLiveMs) ||
                      now - drawnMs_ >= kKeepAliveMs;
    if (!draw) return false;
    key_ = k;  // a live change not drawn yet stays pending until it is
    drawnMs_ = now;
    return true;
}
