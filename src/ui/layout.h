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

// Place menu: row 1 = Source, row 2 = Modulator, row 3 = Processor, each in registry order from
// col 0. Row 0 holds the details of the selected type.
int placeTypeAt(uint8_t row, uint8_t col);                  // index into allTypes(), -1 if none
bool placeCellOf(int typeIdx, uint8_t* row, uint8_t* col);  // false if none or it doesn't fit a row
