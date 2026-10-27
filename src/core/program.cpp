#include "program.h"

Program* compileProgram(const Patch& patch, Module* const* instances, uint32_t generation) {
    Program* prog = new Program();
    prog->generation = generation;
    const int n = patch.moduleCount();
    bool done[kMaxModules] = {};
    int remaining = 0, nextSrc = 0;
    for (int i = 0; i < n; i++) {
        if (instances[i])
            remaining++;
        else
            done[i] = true;  // no instance: skipped, its cables are dropped
    }
    auto indexOf = [&](uint16_t id) {
        for (int i = 0; i < n; i++)
            if (patch.moduleAt(i).id == id) return i;
        return -1;
    };

    while (remaining > 0) {
        int pick = -1, firstLeft = -1;
        for (int i = 0; i < n && pick < 0; i++) {
            if (done[i]) continue;
            if (firstLeft < 0) firstLeft = i;
            bool ready = true;
            for (int c = 0; c < patch.cableCount() && ready; c++) {
                const CableRec& cb = patch.cableAt(c);
                if (cb.toId != patch.moduleAt(i).id || cb.fromId == cb.toId) continue;
                const int f = indexOf(cb.fromId);
                if (f >= 0 && !done[f]) ready = false;
            }
            if (ready) pick = i;
        }
        if (pick < 0) {
            // Stalled: every remaining node waits on another. Break the loop only inside a cycle
            // (transitive closure over remaining nodes), never at a node merely downstream of it.
            bool reach[kMaxModules][kMaxModules] = {};
            for (int c = 0; c < patch.cableCount(); c++) {
                const int f = indexOf(patch.cableAt(c).fromId), t = indexOf(patch.cableAt(c).toId);
                if (f >= 0 && t >= 0 && f != t && !done[f] && !done[t]) reach[f][t] = true;
            }
            for (int k = 0; k < n; k++)
                for (int i = 0; i < n; i++)
                    if (reach[i][k])
                        for (int j = 0; j < n; j++)
                            if (reach[k][j]) reach[i][j] = true;
            for (int i = 0; i < n && pick < 0; i++)
                if (!done[i] && reach[i][i]) pick = i;
            if (pick < 0) pick = firstLeft;  // unreachable; keeps the loop total
        }

        ProgramNode& node = prog->nodes[prog->numNodes++];
        node.m = instances[pick];
        const ModuleRec& rec = patch.moduleAt(pick);
        const int numIns = rec.type->numIns;
        for (int in = 0; in < kMaxIns; in++) {
            node.nSrc[in] = 0;
            node.firstSrc[in] = 0;
        }
        for (int in = 0; in < numIns && in < kMaxIns; in++) {
            node.firstSrc[in] = static_cast<uint16_t>(nextSrc);
            for (int c = 0; c < patch.cableCount(); c++) {
                const CableRec& cb = patch.cableAt(c);
                if (cb.toId != rec.id || cb.toIn != in) continue;
                const int f = indexOf(cb.fromId);
                if (f < 0 || !instances[f]) continue;
                prog->srcs[nextSrc++] = ProgramSource{instances[f], cb.fromOut};
                node.nSrc[in]++;
            }
        }
        done[pick] = true;
        remaining--;
    }
    return prog;
}
