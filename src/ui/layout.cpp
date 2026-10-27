#include "layout.h"
#include "core/units.h"

CellRef moduleCell(const ModuleTypeInfo& t, uint8_t row, uint8_t col) {
    if (row < 1 || row > 3 || col >= kGridCols) return {CellKind::Empty, 0};
    const int usable = t.drawing != Drawing::None ? 5 : kGridCols;
    if (col >= usable) return {CellKind::Drawing, 0};
    if (row == 1 && col < t.numParams) return {CellKind::Param, col};
    if (row == 2 && col < t.numIns) return {CellKind::In, col};
    if (row == 3 && col < t.numOuts) return {CellKind::Out, col};
    return {CellKind::Empty, 0};
}
