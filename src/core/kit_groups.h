#pragma once
#include <cstdint>
#include "patch.h"

// Demand-driven activation (v1.3): a module whose every connected input traces back to one KIT
// trigger lane alone belongs to that lane's group; the engine runs a group only while its lane's
// step sounds. The KIT's ACC output is neutral (a drum fed by T1 and ACC is still T1's), as are
// self-cables and sources with no instance. KIT and OUT never join a group; cycles stay always-on.
constexpr uint8_t kNoGroup = 0xFF;

// groupOf[i] for patch.moduleAt(i): its lane (0..7) or kNoGroup. present[i] false: module i has
// no instance (nullptr = all present). Returns the KIT's patch index, -1 = none.
int kitGroups(const Patch&, uint8_t* groupOf, const bool* present = nullptr);
