#include "layout.h"
#include "core/registry.h"
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

namespace {
uint8_t rowOf(Category c) {
    switch (c) {
        case Category::Source: return 1;
        case Category::Modulator: return 2;
        case Category::Processor: break;
    }
    return 3;
}
}  // namespace

int placeTypeAt(uint8_t row, uint8_t col) {
    if (row < 1 || row > 3 || col >= kGridCols) return -1;
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    int k = 0;
    for (int i = 0; i < n; i++)
        if (rowOf(t[i]->category) == row && k++ == col) return i;
    return -1;
}

bool placeCellOf(int typeIdx, uint8_t* row, uint8_t* col) {
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    if (typeIdx < 0 || typeIdx >= n) return false;
    const uint8_t r = rowOf(t[typeIdx]->category);
    int k = 0;
    for (int i = 0; i < typeIdx; i++)
        if (rowOf(t[i]->category) == r) k++;
    if (k >= kGridCols) return false;
    *row = r;
    *col = (uint8_t)k;
    return true;
}
