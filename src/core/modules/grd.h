#pragma once
#include "../module.h"

// GRD: Plaits macro oscillator (Mutable Instruments, MIT), the engines of a Behringer Grind.
extern const ModuleTypeInfo kGrdType;
int grdEngineIndex(int param);  // ENG param value -> Plaits engine index
