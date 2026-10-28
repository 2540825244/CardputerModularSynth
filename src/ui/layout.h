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

// Module-view value strip: how much of top-row segment col (0..9, each 10% of the knob) is filled
// for a value at norm 0..1, in 0..1. The last segment fills partly, so 35% shows 3.5 segments.
float stripFill(float norm, int col);
