#pragma once
#include <cstdint>
#include "module.h"
#include "patch.h"
#include "units.h"

// Immutable once published: the audio thread only reads it.
struct ProgramSource {
    Module* m;
    uint8_t out;
};

enum class Bypass : uint8_t { None, Pass, Zero };  // Pass: output 0 = input 0, the rest 0

struct ProgramNode {
    Module* m;
    Bypass bypass;
    uint8_t group;  // KIT lane (kit_groups.h) it runs on demand for; kNoGroup = always
    bool boundary;  // grouped, and feeds a module outside its group: its outputs say "still sounding"
    uint8_t nSrc[kMaxIns];
    uint16_t firstSrc[kMaxIns];  // index into Program::srcs
};

struct Program {
    uint32_t generation = 0;
    int numNodes = 0;
    int kitNode = -1;  // the KIT's node, whose events() switch the groups
    ProgramNode nodes[kMaxModules];
    ProgramSource srcs[kMaxCables];
};

// Compiles `patch` using `instances` (instances[i] belongs to patch.moduleAt(i); nullptr = none).
// Order: Kahn's sort on cable dependencies, lowest patch index first; inside a cycle the lowest
// patch index goes first, so feedback cables read the previous block. A bypassed module keeps its
// node (its outputs must read 0 or its input, not the last block it processed) but gets only the
// sources of input 0, and only when it passes them on. Nodes fed only from a KIT lane get its
// group (kit_groups.h).
Program* compileProgram(const Patch& patch, Module* const* instances, uint32_t generation);
