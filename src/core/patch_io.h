#pragma once
#include <cstdint>
#include <string>
#include "engine.h"
#include "patch.h"

constexpr const char* kPatchDir = "/modsynth/patches";
constexpr const char* kSettingsPath = "/modsynth/settings.json";
constexpr int kSlots = 36;
std::string slotPath(int slot);  // 1..36 -> ".../slot01.json"; 0 -> ".../autosave.json"; else ""

// 32 of the widest type (16 params, every amount set) with 64 cables is about 21 KB
// (test_widest_patch_fits). Writers refuse a text over this, so a saved file always loads.
constexpr size_t kMaxPatchJson = 28672;
std::string patchToJson(const Patch&);
inline bool patchFits(const std::string& json) { return json.size() <= kMaxPatchJson; }

enum class LoadError : uint8_t { None, NoFile, BadJson, NewerFormat, Empty };
struct LoadReport {
    LoadError error;
    int skippedModules;
    int skippedCables;
};

// `out` is untouched unless error == None. `budget` (optional) vetoes modules via canAdd() against
// the partially built patch.
LoadReport patchFromJson(const std::string&, Patch& out, const Engine* budget);
