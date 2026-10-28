#include "registry.h"
#include <cstring>
#include "modules/dly.h"
#include "modules/env.h"
#include "modules/keys.h"
#include "modules/atn.h"
#include "modules/lfo.h"
#include "modules/mix.h"
#include "modules/out.h"
#include "modules/osc.h"
#include "modules/vca.h"
#include "modules/vcf.h"

// One line per module type, in place-menu order.
static const ModuleTypeInfo* const kTypes[] = {
    &kKeysType,
    &kOscType,
    &kVcfType,
    &kVcaType,
    &kEnvType,
    &kLfoType,
    &kDlyType,
    &kOutType,
    &kMixType,
    &kAtnType,
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
