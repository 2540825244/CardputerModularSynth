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
    TEST_ASSERT_TRUE(u.pop(p));
    TEST_ASSERT_EQUAL(1, p.moduleCount());
    TEST_ASSERT_FALSE(u.pop(p));
}

void test_depth() {
    UndoStack u;
    for (int i = 0; i < 15; i++) u.push(withModules(i));
    TEST_ASSERT_EQUAL(kUndoDepth, u.size());
    Patch p;
    for (int i = 14; i >= 5; i--) {
        TEST_ASSERT_TRUE(u.pop(p));
        TEST_ASSERT_EQUAL(i, p.moduleCount());
    }
    TEST_ASSERT_FALSE(u.pop(p));
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
    u.pop(p);
    TEST_ASSERT_EQUAL(8, p.moduleCount());
    u.push(withModules(9), paramKey(3, 2, false));  // popping also breaks the run
    TEST_ASSERT_EQUAL(3, u.size());
    TEST_ASSERT_TRUE(paramKey(1, 0, false) != paramKey(1, 0, true));
    TEST_ASSERT_TRUE(paramKey(0, 0, false) != 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_undo_restores);
    RUN_TEST(test_depth);
    RUN_TEST(test_coalesce);
    return UNITY_END();
}
