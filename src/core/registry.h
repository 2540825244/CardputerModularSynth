#pragma once
#include "module.h"

// Registry order = place-menu order: keys, osc, vcf, vca, env, lfo, dly, out.
const ModuleTypeInfo* const* allTypes(int* count);
const ModuleTypeInfo* findType(const char* id);  // nullptr if unknown
