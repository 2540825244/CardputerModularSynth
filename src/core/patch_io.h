#pragma once
#include <cstdint>
#include <string>
#include "engine.h"
#include "patch.h"

constexpr const char* kPatchDir = "/modsynth/patches";
std::string slotPath(int slot);  // 1..12 -> ".../slot01.json"; 0 -> ".../autosave.json"; else ""

std::string patchToJson(const Patch&);

enum class LoadError : uint8_t { None, NoFile, BadJson, NewerFormat, Empty };
struct LoadReport {
    LoadError error;
    int skippedModules;
    int skippedCables;
};

// `out` is untouched unless error == None. `budget` (optional) vetoes modules via canAdd() against
// the partially built patch.
LoadReport patchFromJson(const std::string&, Patch& out, const Engine* budget);
