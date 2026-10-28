#pragma once
#include "../module.h"

// Modulators, sequencers and utilities (v1.2).
extern const ModuleTypeInfo kAdType, kRndType, kSeqType, kTrgType, kEucType, kQntType, kTurType, kLgcType,
    kBrnType, kChsType, kKnbType, kScoType;
bool euclidHit(int steps, int fills, int rot, int i);  // EUC: is step i a hit
