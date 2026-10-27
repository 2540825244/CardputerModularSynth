#include "registry.h"
#include <cstring>
#include "modules/env.h"
#include "modules/lfo.h"
#include "modules/osc.h"
#include "modules/vca.h"
#include "modules/vcf.h"

// One line per module type, in place-menu order.
static const ModuleTypeInfo* const kTypes[] = {
    &kOscType,
    &kVcfType,
    &kVcaType,
    &kEnvType,
    &kLfoType,
};

const ModuleTypeInfo* const* allTypes(int* count) {
    if (count) *count = (int)(sizeof(kTypes) / sizeof(kTypes[0]));
    return kTypes;
}

const ModuleTypeInfo* findType(const char* id) {
    if (!id) return nullptr;
    for (const ModuleTypeInfo* t : kTypes)
        if (strcmp(t->id, id) == 0) return t;
    return nullptr;
}
