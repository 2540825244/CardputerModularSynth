#pragma once
#include <cstdint>
#include "core/module.h"

enum class CellKind : uint8_t { Empty, Param, In, Out, Drawing };
struct CellRef {
    CellKind kind;
    uint8_t index;
};

// Module-view cell layout. Rows 1..3 (row 0 is the value strip): params on row 1, ins on row 2,
// outs on row 3, each from col 0. With a drawing, cols 5..11 are Drawing and pins fit in cols 0..4.
CellRef moduleCell(const ModuleTypeInfo&, uint8_t row, uint8_t col);
