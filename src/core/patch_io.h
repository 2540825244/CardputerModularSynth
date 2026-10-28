#pragma once
#include <cstdint>
#include <string>
#include "engine.h"
#include "patch.h"

constexpr const char* kPatchDir = "/modsynth/patches";
constexpr const char* kSettingsPath = "/modsynth/settings.json";
constexpr int kSlots = 36;
std::string slotPath(int slot);  // 1..36 -> ".../slot01.json"; 0 -> ".../autosave.json"; else ""

// 32 of the widest type (22 params, every amount set) with 64 cables fits
// (test_widest_patch_fits). Writers refuse a text over this, so a saved file always loads.
constexpr size_t kMaxPatchJson = 28672;
// "" when the heap ran out: ArduinoJson drops what it cannot store, so a truncated-but-valid text
// would otherwise be written over a good file. Callers treat "" as "NO RAM".
std::string patchToJson(const Patch&);
inline bool patchFits(const std::string& json) { return json.size() <= kMaxPatchJson; }

enum class LoadError : uint8_t { None, NoFile, BadJson, NewerFormat, Empty, NoMemory };
struct LoadReport {
    LoadError error;
    int skippedModules;
    int skippedCables;
    const char* skippedType;  // the first type the budget refused (NO BUFFER / MAX 1), or nullptr
};

// `out` is untouched unless error == None. `budget` (optional) vetoes modules via canLoad() against
// the partially built patch (pool slots, instance counts; never CPU or RAM).
LoadReport patchFromJson(const std::string&, Patch& out, const Engine* budget);
