#pragma once
#include <cstdint>
#include "core/module.h"

// Module-view pages. Row 0 is the value strip; rows 1..3 hold cells.
// Automatic pages (type.pages == nullptr): params on row 1, ins on row 2, outs on row 3, each from
// col 0. Page 0 is 5 cells wide when the type has a drawing (cols 5..11 draw), else 12; later pages
// are 12 wide and continue each list where the previous page stopped.
int modulePageCount(const ModuleTypeInfo&);
const char* modulePageName(const ModuleTypeInfo&, int page);  // "MAIN", "P2", ... or the PageDef name
PageKind modulePageKind(const ModuleTypeInfo&, int page);
Drawing modulePageDrawing(const ModuleTypeInfo&, int page);
CellRef moduleCell(const ModuleTypeInfo&, int page, uint8_t row, uint8_t col);
inline CellRef moduleCell(const ModuleTypeInfo& t, uint8_t row, uint8_t col) { return moduleCell(t, 0, row, col); }

// Place menu: one page per Category (Source, Modulator, Processor, Effect, Utility). A page's
// types fill rows 1..3 in registry order, 12 per row. Row 0 holds the details of the selected type.
int placePageCount();
const char* placePageName(int page);                                // "SRC", "MOD", "PRC", "FX", "UTL"
int placeTypeAt(int page, uint8_t row, uint8_t col);                // index into allTypes(), -1 if none
bool placeCellOf(int typeIdx, int* page, uint8_t* row, uint8_t* col);  // false if none or > 36 in a page

// Module-view value strip: how much of top-row segment col (0..9, each 10% of the knob) is filled
// for a value at norm 0..1, in 0..1. The last segment fills partly, so 35% shows 3.5 segments.
float stripFill(float norm, int col);
