#include <unity.h>
#include <cstring>
#include "core/modules/grd.h"
#include "core/modules/osc.h"
#include "core/undo.h"

void setUp() {}
void tearDown() {}

// One OSC whose first param tags the step: small enough that a full history fits.
static Patch tagged(int i) {
    Patch p;
    uint16_t id = 0;
    p.addModule(kOscType, 0, 0, &id);
    p.find(id)->params[0] = (float)i;
    return p;
}

static int tagOf(const Patch& p) { return (int)p.moduleAt(0).params[0]; }

static Patch withModules(int n) {
    Patch p;
    for (int i = 0; i < n; i++) p.addModule(kOscType, 0, i, nullptr);
    return p;
}

void test_undo_restores() {
    UndoStack u;
    Patch p = withModules(1);
    u.push(p);
    p.addModule(kOscType, 0, 5, nullptr);
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_EQUAL(1, p.moduleCount());
    TEST_ASSERT_FALSE(u.undo(p));
}

void test_depth() {
    UndoStack u;
    const int n = kUndoDepth + 7;
    for (int i = 0; i < n; i++) u.push(tagged(i));
    TEST_ASSERT_EQUAL(kUndoDepth, u.size());
    Patch p = tagged(n);
    for (int i = n - 1; i >= n - kUndoDepth; i--) {
        TEST_ASSERT_TRUE(u.undo(p));
        TEST_ASSERT_EQUAL(i, tagOf(p));
    }
    TEST_ASSERT_FALSE(u.undo(p));
}

void test_coalesce() {
    UndoStack u;
    const uint32_t k = paramKey(3, 1, false);
    for (int i = 0; i < 5; i++) u.push(withModules(i), k);
    TEST_ASSERT_EQUAL(1, u.size());
    u.push(withModules(6), paramKey(3, 2, false));
    TEST_ASSERT_EQUAL(2, u.size());
    u.push(withModules(7), paramKey(3, 2, false));
    TEST_ASSERT_EQUAL(2, u.size());
    u.breakCoalescing();
    u.push(withModules(8), paramKey(3, 2, false));
    TEST_ASSERT_EQUAL(3, u.size());
    Patch p;
    u.undo(p);
    TEST_ASSERT_EQUAL(8, p.moduleCount());
    u.push(withModules(9), paramKey(3, 2, false));  // popping also breaks the run
    TEST_ASSERT_EQUAL(3, u.size());
    TEST_ASSERT_TRUE(paramKey(1, 0, false) != paramKey(1, 0, true));
    TEST_ASSERT_TRUE(paramKey(0, 0, false) != 0);
}


void test_undo_redo() {
    UndoStack u;
    Patch p = withModules(1);
    u.push(p);  // before adding the 2nd
    p = withModules(2);
    u.push(p);  // before adding the 3rd
    p = withModules(3);
    TEST_ASSERT_FALSE(u.redo(p));
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_EQUAL(1, p.moduleCount());
    TEST_ASSERT_EQUAL(2, u.redoSize());
    TEST_ASSERT_TRUE(u.redo(p));
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_TRUE(u.redo(p));
    TEST_ASSERT_EQUAL(3, p.moduleCount());
    TEST_ASSERT_FALSE(u.redo(p));
    TEST_ASSERT_TRUE(u.undo(p));
    u.push(p);  // a new edit drops the redo tail
    p = withModules(5);
    TEST_ASSERT_EQUAL(0, u.redoSize());
    TEST_ASSERT_FALSE(u.redo(p));
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_EQUAL(2, p.moduleCount());
}

void test_redo_after_full_ring() {
    UndoStack u;
    Patch p = tagged(0);
    const int n = kUndoDepth + 7;
    for (int i = 0; i < n; i++) {
        u.push(p);
        p = tagged(i + 1);
    }
    for (int i = 0; i < kUndoDepth; i++) TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_FALSE(u.undo(p));
    TEST_ASSERT_EQUAL(n - kUndoDepth, tagOf(p));
    for (int i = 0; i < kUndoDepth; i++) TEST_ASSERT_TRUE(u.redo(p));
    TEST_ASSERT_EQUAL(n, tagOf(p));
}

// The biggest patch: 32 GRD-shaped voices (22 params, 9 ins, 4 outs) and 64 cables.
static const ModuleTypeInfo& bigType() {
    static const ModuleTypeInfo t = [] {
        ModuleTypeInfo x = kGrdType;
        x.maxInstances = 0;
        return x;
    }();
    return t;
}

static Patch full() {
    Patch p;
    for (int i = 0; i < kMaxModules; i++) {
        uint16_t id = 0;
        p.addModule(bigType(), (uint8_t)(i / kCellsPerPage), (uint8_t)(i % kCellsPerPage), &id);
        ModuleRec* r = p.find(id);
        for (int k = 0; k < kMaxParams; k++) r->params[k] = 0.01f * (float)(i * 31 + k);
        r->inAmount[i % kMaxIns] = -0.5f;
    }
    for (int c = 0; c < kMaxCables; c++)
        p.addCable(p.moduleAt(c % kMaxModules).id, (uint8_t)(c % 4), p.moduleAt((c + 1) % kMaxModules).id,
                   (uint8_t)((c / kMaxModules) % kMaxIns));
    return p;
}

static bool same(const Patch& a, const Patch& b) { return memcmp(&a, &b, sizeof(Patch)) == 0; }

void test_snapshots_are_compact() {
    // Only the modules in use are stored, each with its own type's params and pins.
    TEST_ASSERT_TRUE(sizeof(UndoStack) < kUndoBytes + 1024);
    UndoStack u;
    Patch p = withModules(8);
    for (int i = 0; i < kUndoDepth; i++) u.push(p);
    TEST_ASSERT_EQUAL(kUndoDepth, u.size());  // eight small modules: a full history fits
}

void test_full_patch_round_trips() {
    const Patch big = full();
    TEST_ASSERT_EQUAL(kMaxModules, big.moduleCount());
    TEST_ASSERT_EQUAL(kMaxCables, big.cableCount());
    UndoStack u;
    u.push(big);
    Patch p = withModules(2);
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_TRUE(same(p, big));
    TEST_ASSERT_TRUE(u.redo(p));
    TEST_ASSERT_TRUE(same(p, withModules(2)));
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_TRUE(same(p, big));
}

void test_big_patches_drop_the_oldest() {
    // Each full snapshot is ~5 KB: the history shrinks, the newest step always survives.
    UndoStack u;
    Patch p = full();
    for (int i = 0; i < 5; i++) {
        u.push(p);
        p.setCableColor(0, (uint8_t)(i + 1));
    }
    TEST_ASSERT_TRUE(u.size() >= 1);
    TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_EQUAL(4, p.cableAt(0).color);
    // Undo on a full arena still keeps the state it left for redo.
    while (u.undo(p)) {}
    while (u.redo(p)) {}
    TEST_ASSERT_EQUAL(5, p.cableAt(0).color);
}

void test_small_undo_after_big_edits() {
    UndoStack u;
    u.push(withModules(1));
    for (int i = 0; i < 3; i++) u.push(full());
    Patch p = full();
    int steps = 0;
    while (u.undo(p)) steps++;
    TEST_ASSERT_TRUE(steps >= 1);
    TEST_ASSERT_TRUE(p.moduleCount() == kMaxModules || p.moduleCount() == 1);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_undo_restores);
    RUN_TEST(test_depth);
    RUN_TEST(test_coalesce);
    RUN_TEST(test_undo_redo);
    RUN_TEST(test_redo_after_full_ring);
    RUN_TEST(test_snapshots_are_compact);
    RUN_TEST(test_full_patch_round_trips);
    RUN_TEST(test_big_patches_drop_the_oldest);
    RUN_TEST(test_small_undo_after_big_edits);
    return UNITY_END();
}
