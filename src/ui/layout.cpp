#include "layout.h"
#include "core/registry.h"
#include "core/units.h"

namespace {

int width0(const ModuleTypeInfo& t) { return t.drawing != Drawing::None ? 5 : kGridCols; }
int offsetOf(const ModuleTypeInfo& t, int page) { return page == 0 ? 0 : width0(t) + (page - 1) * kGridCols; }
int pagesFor(const ModuleTypeInfo& t, int n) {
    const int w0 = width0(t);
    return n <= w0 ? 1 : 1 + (n - w0 + kGridCols - 1) / kGridCols;
}

}  // namespace

int modulePageCount(const ModuleTypeInfo& t) {
    if (t.pages) return t.numPages;
    int n = pagesFor(t, t.numParams);
    if (pagesFor(t, t.numIns) > n) n = pagesFor(t, t.numIns);
    if (pagesFor(t, t.numOuts) > n) n = pagesFor(t, t.numOuts);
    return n;
}

const char* modulePageName(const ModuleTypeInfo& t, int page) {
    if (t.pages && page >= 0 && page < t.numPages) return t.pages[page].name;
    static const char* const kNames[] = {"MAIN", "P2", "P3", "P4", "P5"};
    return kNames[page < 0 ? 0 : (page > 4 ? 4 : page)];
}

PageKind modulePageKind(const ModuleTypeInfo& t, int page) {
    if (t.pages && page >= 0 && page < t.numPages) return t.pages[page].kind;
    return PageKind::Cells;
}

Drawing modulePageDrawing(const ModuleTypeInfo& t, int page) {
    if (t.pages) return page >= 0 && page < t.numPages ? t.pages[page].drawing : Drawing::None;
    return page == 0 ? t.drawing : Drawing::None;
}

CellRef moduleCell(const ModuleTypeInfo& t, int page, uint8_t row, uint8_t col) {
    if (row < 1 || row > 3 || col >= kGridCols || page < 0 || page >= modulePageCount(t)) return c_;
    if (t.pages) {
        const PageDef& p = t.pages[page];
        return p.cells ? p.cells[(row - 1) * kGridCols + col] : c_;
    }
    const int usable = page == 0 ? width0(t) : kGridCols;
    if (col >= usable) return cD;
    const int i = offsetOf(t, page) + col;
    if (row == 1 && i < t.numParams) return cP((uint8_t)i);
    if (row == 2 && i < t.numIns) return cI((uint8_t)i);
    if (row == 3 && i < t.numOuts) return cO((uint8_t)i);
    return c_;
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

float stripFill(float norm, int col) {
    if (col < 0 || col >= 10) return 0.f;
    float f = norm * 10.f - (float)col;
    // Snap float noise, so a value on a 10% step is exactly full with no sliver after it.
    if (f < 1e-3f) return 0.f;
    if (f > 1.f - 1e-3f) return 1.f;
    return f;
}
