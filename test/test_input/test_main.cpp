#include <unity.h>
#include "ui/key_input.h"

void setUp() {}
void tearDown() {}

static int step(KeyDecoder& d, const KeyPos* k, int n, uint32_t t, KeyEvent* ev) {
    int c = 0;
    d.update(k, n, t, ev, &c, 16);
    return c;
}

void test_mapping() {
    KeyDecoder d;
    KeyEvent ev[16];
    KeyPos a[] = {{0, 2}};
    TEST_ASSERT_EQUAL(1, step(d, a, 1, 0, ev));
    TEST_ASSERT_FALSE(ev[0].grid);
    TEST_ASSERT_EQUAL(Side::Fn, ev[0].side);
    KeyPos b[] = {{13, 3}};
    step(d, b, 1, 1, ev);  // Fn up, Space down
    bool sawSpace = false;
    for (int i = 0; i < 2; i++)
        if (!ev[i].grid && ev[i].type == KeyEvent::Down && ev[i].side == Side::Space) sawSpace = true;
    TEST_ASSERT_TRUE(sawSpace);
    KeyPos c[] = {{1, 0}};
    step(d, c, 1, 2, ev);
    TEST_ASSERT_TRUE(d.gridHeld(0, 0));
    KeyPos e[] = {{12, 3}};
    step(d, e, 1, 3, ev);
    TEST_ASSERT_TRUE(d.gridHeld(11, 3));
    TEST_ASSERT_FALSE(d.gridHeld(0, 0));
}

void test_down_up() {
    KeyDecoder d;
    KeyEvent ev[16];
    TEST_ASSERT_EQUAL(0, step(d, nullptr, 0, 0, ev));
    KeyPos k[] = {{5, 1}};
    TEST_ASSERT_EQUAL(1, step(d, k, 1, 10, ev));
    TEST_ASSERT_EQUAL(KeyEvent::Down, ev[0].type);
    TEST_ASSERT_TRUE(ev[0].grid);
    TEST_ASSERT_EQUAL(4, ev[0].col);
    TEST_ASSERT_EQUAL(1, ev[0].row);
    TEST_ASSERT_EQUAL(1, step(d, nullptr, 0, 20, ev));
    TEST_ASSERT_EQUAL(KeyEvent::Up, ev[0].type);
    TEST_ASSERT_EQUAL(4, ev[0].col);
}

void test_hold_once() {
    KeyDecoder d;
    KeyEvent ev[16];
    KeyPos k[] = {{5, 1}};
    step(d, k, 1, 0, ev);
    TEST_ASSERT_EQUAL(0, step(d, k, 1, 399, ev));
    TEST_ASSERT_EQUAL(1, step(d, k, 1, 400, ev));
    TEST_ASSERT_EQUAL(KeyEvent::Hold, ev[0].type);
    int holds = 0;
    for (uint32_t t = 401; t <= 900; t += 50) holds += step(d, k, 1, t, ev);
    TEST_ASSERT_EQUAL(0, holds);
}

void test_chord() {
    KeyDecoder d;
    KeyEvent ev[16];
    KeyPos k[] = {{0, 2}, {2, 1}};
    step(d, k, 2, 0, ev);
    TEST_ASSERT_TRUE(d.sideHeld(Side::Fn));
    TEST_ASSERT_TRUE(d.gridHeld(1, 1));
    int n = step(d, nullptr, 0, 0, ev);  // release all
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_FALSE(d.sideHeld(Side::Fn));
}

void test_chord_events() {
    KeyDecoder d;
    KeyEvent ev[16];
    KeyPos k[] = {{0, 2}, {2, 1}};
    int n = step(d, k, 2, 0, ev);
    TEST_ASSERT_EQUAL(2, n);
    bool grid = false;
    for (int i = 0; i < n; i++)
        if (ev[i].grid && ev[i].col == 1 && ev[i].row == 1 && ev[i].type == KeyEvent::Down) grid = true;
    TEST_ASSERT_TRUE(grid);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mapping);
    RUN_TEST(test_down_up);
    RUN_TEST(test_hold_once);
    RUN_TEST(test_chord);
    RUN_TEST(test_chord_events);
    return UNITY_END();
}
