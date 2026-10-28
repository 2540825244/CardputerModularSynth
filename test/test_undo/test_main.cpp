#include <unity.h>
#include "core/modules/osc.h"
#include "core/undo.h"

void setUp() {}
void tearDown() {}

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
    for (int i = 0; i < 15; i++) u.push(withModules(i));
    TEST_ASSERT_EQUAL(kUndoDepth, u.size());
    Patch p;
    for (int i = 14; i >= 15 - kUndoDepth; i--) {
        TEST_ASSERT_TRUE(u.undo(p));
        TEST_ASSERT_EQUAL(i, p.moduleCount());
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
    Patch p;
    for (int i = 0; i < 15; i++) {
        u.push(p);
        p = withModules(i + 1);
    }
    for (int i = 0; i < kUndoDepth; i++) TEST_ASSERT_TRUE(u.undo(p));
    TEST_ASSERT_FALSE(u.undo(p));
    TEST_ASSERT_EQUAL(15 - kUndoDepth, p.moduleCount());
    for (int i = 0; i < kUndoDepth; i++) TEST_ASSERT_TRUE(u.redo(p));
    TEST_ASSERT_EQUAL(15, p.moduleCount());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_undo_restores);
    RUN_TEST(test_depth);
    RUN_TEST(test_coalesce);
    RUN_TEST(test_undo_redo);
    RUN_TEST(test_redo_after_full_ring);
    return UNITY_END();
}
