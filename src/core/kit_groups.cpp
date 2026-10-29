#include "kit_groups.h"
#include "modules/kit.h"
#include "modules/out.h"

int kitGroups(const Patch& p, uint8_t* g, const bool* present) {
    const int n = p.moduleCount();
    int kit = -1;
    for (int i = 0; i < n; i++) {
        g[i] = kNoGroup;
        if (p.moduleAt(i).type == &kKitType && (!present || present[i])) kit = i;
    }
    if (kit < 0) return -1;
    auto indexOf = [&](uint16_t id) {
        for (int i = 0; i < n; i++)
            if (p.moduleAt(i).id == id) return i;
        return -1;
    };
    // Every module starts always-on; one joins lane L once all its sources are lane L (the KIT's
    // trigger L, or modules already in L). Sources only ever join, so this settles in n passes,
    // and a cycle never does (each member waits on the other).
    for (bool changed = true; changed;) {
        changed = false;
        for (int i = 0; i < n; i++) {
            const ModuleRec& r = p.moduleAt(i);
            if (g[i] != kNoGroup || i == kit || r.type == &kOutType || (present && !present[i])) continue;
            uint8_t lane = kNoGroup;
            bool ok = true;
            for (int c = 0; c < p.cableCount() && ok; c++) {
                const CableRec& cb = p.cableAt(c);
                if (cb.toId != r.id || cb.fromId == r.id) continue;
                const int f = indexOf(cb.fromId);
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
            }
            if (ok && lane != kNoGroup) {
                g[i] = lane;
                changed = true;
            }
        }
    }
    return kit;
}
