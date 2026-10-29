#include "kit_groups.h"
#include <cstring>
#include "modules/kit.h"
#include "modules/out.h"

namespace {

int indexOf(const Patch& p, uint16_t id) {
    for (int i = 0; i < p.moduleCount(); i++)
        if (p.moduleAt(i).id == id) return i;
    return -1;
}

// A source (oscillator, voice) rests only when struck: the lane must reach its trigger or gate.
bool strikes(const ModuleTypeInfo& t, int in) {
    if (t.category != Category::Source) return true;
    return in < t.numIns && (!strcmp(t.inKeys[in], "trig") || !strcmp(t.inKeys[in], "gate"));
}

// Every module starts always-on; one joins lane L once all its sources are lane L (the KIT's
// trigger L, or modules already in L) and, for a source, one of them strikes it. Sources only ever
// join, so this settles in n passes, and a cycle never does (each member waits on the other).
void settle(const Patch& p, uint8_t* g, const bool* present, const bool* never, int kit) {
    const int n = p.moduleCount();
    for (int i = 0; i < n; i++) g[i] = kNoGroup;
    for (bool changed = true; changed;) {
        changed = false;
        for (int i = 0; i < n; i++) {
            const ModuleRec& r = p.moduleAt(i);
            if (g[i] != kNoGroup || never[i]) continue;
            uint8_t lane = kNoGroup;
            bool ok = true, struck = false;
            for (int c = 0; c < p.cableCount() && ok; c++) {
                const CableRec& cb = p.cableAt(c);
                if (cb.toId != r.id || cb.fromId == r.id) continue;
                const int f = indexOf(p, cb.fromId);
                if (f < 0 || (present && !present[f])) continue;
                uint8_t l;
                if (f == kit) {
                    if (cb.fromOut >= kit::kLanes) continue;  // ACC: neutral
                    l = cb.fromOut;
                } else {
                    l = g[f];
                }
                if (l == kNoGroup || (lane != kNoGroup && l != lane)) ok = false;
                lane = l;
                struck = struck || strikes(*r.type, cb.toIn);
            }
            if (ok && struck && lane != kNoGroup) {
                g[i] = lane;
                changed = true;
            }
        }
    }
}

}  // namespace

int kitGroups(const Patch& p, uint8_t* g, const bool* present) {
    const int n = p.moduleCount();
    int kit = -1;
    bool never[kMaxModules];
    for (int i = 0; i < n; i++) {
        g[i] = kNoGroup;
        const ModuleTypeInfo& t = *p.moduleAt(i).type;
        if (&t == &kKitType && (!present || present[i])) kit = i;
        // A switched-off group must sound like silence: no OUT, no delay/reverb (its echo outlives
        // the 50 ms quiet window), nothing without an instance.
        never[i] = &t == &kKitType || &t == &kOutType || t.poolSlots || (present && !present[i]);
    }
    if (kit < 0) return -1;
    // A module whose output leaves its group must be audio (source, processor, effect): a held
    // CV (a SEQ tuning an always-on OSC) would drop to 0 when the group stops. Such a module runs
    // always; that can pull others out too, so settle again until nothing changes.
    for (bool again = true; again;) {
        settle(p, g, present, never, kit);
        again = false;
        for (int c = 0; c < p.cableCount(); c++) {
            const CableRec& cb = p.cableAt(c);
            const int f = indexOf(p, cb.fromId), t = indexOf(p, cb.toId);
            if (f < 0 || t < 0 || f == t || g[f] == kNoGroup || g[t] == g[f]) continue;
            if (present && !present[t]) continue;
            const ModuleTypeInfo& ft = *p.moduleAt(f).type;
            const Category cat = ft.category;
            const char* pin = cb.fromOut < ft.numOuts ? ft.outKeys[cb.fromOut] : "";
            const bool cv = cat == Category::Modulator || cat == Category::Utility ||
                            (cat == Category::Source && strcmp(pin, "out") && strcmp(pin, "aux"));  // GRD env/lfo
            if (cv) {
                never[f] = true;
                again = true;
            }
        }
    }
    return kit;
}
