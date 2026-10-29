#include "layout.h"
#include "core/registry.h"
#include "core/units.h"

namespace {

int width0(const ModuleTypeInfo& t) { return t.drawing != Drawing::None ? 5 : kGridCols; }
int offsetOf(const ModuleTypeInfo& t, int page) { return page == 0 ? 0 : width0(t) + (page - 1) * kGridCols; }
// v1.4: with a drawing, params past row 1 continue on row 3 after the outs (cols numOuts..4), so
// most modules keep every knob next to their picture.
int params0(const ModuleTypeInfo& t) {
    if (t.drawing == Drawing::None) return kGridCols;
    return 5 + (t.numOuts < 5 ? 5 - t.numOuts : 0);
}
int paramOffset(const ModuleTypeInfo& t, int page) { return page == 0 ? 0 : params0(t) + (page - 1) * kGridCols; }
int pagesFor(int n, int first) { return n <= first ? 1 : 1 + (n - first + kGridCols - 1) / kGridCols; }
int pagesFor(const ModuleTypeInfo& t, int n) { return pagesFor(n, width0(t)); }

}  // namespace

int modulePageCount(const ModuleTypeInfo& t) {
    if (t.pages) return t.numPages;
    int n = pagesFor(t.numParams, params0(t));
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
    const int i = offsetOf(t, page) + col, pi = paramOffset(t, page) + col;
    if (row == 1 && pi < t.numParams) return cP((uint8_t)pi);
    if (row == 2 && i < t.numIns) return cI((uint8_t)i);
    if (row == 3 && i < t.numOuts) return cO((uint8_t)i);
    if (row == 3 && page == 0 && t.drawing != Drawing::None) {  // overflow params after the outs
        const int k = 5 + col - t.numOuts;
        if (k < t.numParams) return cP((uint8_t)k);
    }
    return c_;
}

int placePageCount() { return 5; }

const char* placePageName(int page) {
    static const char* const kNames[] = {"SRC", "MOD", "PRC", "FX", "UTL"};
    return page >= 0 && page < 5 ? kNames[page] : "";
}

int placeTypeAt(int page, uint8_t row, uint8_t col) {
    if (row < 1 || row > 3 || col >= kGridCols || page < 0 || page >= placePageCount()) return -1;
    const int want = (row - 1) * kGridCols + col;
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    int k = 0;
    for (int i = 0; i < n; i++)
        if ((int)t[i]->category == page && k++ == want) return i;
    return -1;
}

bool placeCellOf(int typeIdx, int* page, uint8_t* row, uint8_t* col) {
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    if (typeIdx < 0 || typeIdx >= n) return false;
    const int pg = (int)t[typeIdx]->category;
    int k = 0;
    for (int i = 0; i < typeIdx; i++)
        if ((int)t[i]->category == pg) k++;
    if (k >= 3 * kGridCols) return false;
    *page = pg;
    *row = (uint8_t)(1 + k / kGridCols);
    *col = (uint8_t)(k % kGridCols);
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
