#include "registry.h"
#include <cstring>
#include "modules/dly.h"
#include "modules/fx.h"
#include "modules/proc.h"
#include "modules/src.h"
#include "modules/kit.h"
#include "modules/mod.h"
#include "modules/grd.h"
#include "modules/dmx.h"
#include "modules/env.h"
#include "modules/keys.h"
#include "modules/atn.h"
#include "modules/clk.h"
#include "modules/lfo.h"
#include "modules/mix.h"
#include "modules/mux.h"
#include "modules/out.h"
#include "modules/snh.h"
#include "modules/tilt.h"
#include "modules/osc.h"
#include "modules/vca.h"
#include "modules/vcf.h"
#include "modules/v14.h"
#include "modules/seq2.h"

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
    &kGrdType,  // after the v1 eight (tests index those); third on the Sources page
    &kMixType,
    &kAtnType,
    &kSnhType,
    &kClkType,
    &kMuxType,
    &kDmxType,
    &kTiltType,
    // v1.2 effects
    &kRevType,
    &kChoType,
    &kFlgType,
    &kPhaType,
    &kTrmType,
    &kPshType,
    // v1.2 processors and utilities
    &kLadType,
    &kFldType,
    &kOvdType,
    &kCruType,
    &kLpgType,
    &kRingType,
    &kXfdType,
    &kDynType,
    &kAwhType,
    &kEnfType,
    &kSlwType,
    // v1.2 sources
    &kNoiType,
    &kVsoType,
    &kSswType,
    &kFmoType,
    &kForType,
    &kVosType,
    &kZosType,
    &kHrmType,
    &kStrType,
    &kMdlType,
    &kResType,
    &kKckType,
    &kSnrType,
    &kHatType,
    &kPrtType,
    // v1.2 modulators, sequencers, utilities
    &kAdType,
    &kRndType,
    &kSeqType,
    &kTrgType,
    &kKitType,
    &kEucType,
    &kTurType,
    &kChsType,
    &kQntType,
    &kLgcType,
    &kBrnType,
    &kKnbType,
    &kScoType,
    // v1.4
    &kArpType,
    &kClpType,
    &kPrcType,
    &kVowType,
    &kEq3Type,
    // v1.5
    &kRolType,
    &kEvoType,
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
