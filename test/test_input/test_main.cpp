#include <unity.h>
#include "ui/gesture.h"
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

// ---- gesture mapper ----
static Action A[2];
static KeyEvent G(KeyEvent::Type t, int c, int r) {
    KeyEvent e{};
    e.type = t;
    e.grid = true;
    e.col = (uint8_t)c;
    e.row = (uint8_t)r;
    return e;
}
static KeyEvent S(KeyEvent::Type t, Side s) {
    KeyEvent e{};
    e.type = t;
    e.side = s;
    return e;
}

void test_tap_selects_only() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Down, 2, 1), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Select, A[0].act);
    TEST_ASSERT_EQUAL(14, A[0].cell);
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Up, 2, 1), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Key, A[0].act);
    TEST_ASSERT_EQUAL(KeyEvent::Up, A[0].key.type);
}

void test_fn_tap_selects_and_activates() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(2, g.map(G(KeyEvent::Down, 2, 1), true, false, false, A));
    TEST_ASSERT_EQUAL(Act::Select, A[0].act);
    TEST_ASSERT_EQUAL(14, A[0].cell);
    TEST_ASSERT_EQUAL(Act::Activate, A[1].act);
    TEST_ASSERT_EQUAL(14, A[1].cell);
}

void test_hold_activates() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Down, 0, 3), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Select, A[0].act);
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Hold, 0, 3), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Activate, A[0].act);
    TEST_ASSERT_EQUAL(36, A[0].cell);
}

void test_fn_down_then_hold_activates_once() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(2, g.map(G(KeyEvent::Down, 5, 2), true, false, false, A));
    TEST_ASSERT_EQUAL(0, g.map(G(KeyEvent::Hold, 5, 2), true, false, false, A));
    g.map(G(KeyEvent::Up, 5, 2), false, false, false, A);
    // the next press of the same key is a fresh gesture
    g.map(G(KeyEvent::Down, 5, 2), false, false, false, A);
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Hold, 5, 2), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Activate, A[0].act);
}

void test_ctrl_hold_does_not_activate() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Down, 1, 1), false, true, false, A));
    TEST_ASSERT_EQUAL(Act::Select, A[0].act);
    TEST_ASSERT_EQUAL(0, g.map(G(KeyEvent::Hold, 1, 1), false, true, false, A));
}

void test_enter_tap_activates_selection() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(0, g.map(S(KeyEvent::Down, Side::Enter), false, false, false, A));
    TEST_ASSERT_EQUAL(1, g.map(S(KeyEvent::Up, Side::Enter), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Activate, A[0].act);
    TEST_ASSERT_EQUAL(-1, A[0].cell);
}

void test_enter_hold_sweeps() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(0, g.map(S(KeyEvent::Down, Side::Enter), false, false, false, A));
    TEST_ASSERT_EQUAL(1, g.map(S(KeyEvent::Hold, Side::Enter), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Sweep, A[0].act);
    TEST_ASSERT_EQUAL(1, g.map(S(KeyEvent::Up, Side::Enter), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::SweepEnd, A[0].act);
    // a later tap is a plain activate again
    g.map(S(KeyEvent::Down, Side::Enter), false, false, false, A);
    g.map(S(KeyEvent::Up, Side::Enter), false, false, false, A);
    TEST_ASSERT_EQUAL(Act::Activate, A[0].act);
}

void test_raw_mode_passes_everything() {
    GestureMapper g;
    const KeyEvent evs[] = {G(KeyEvent::Down, 3, 2), G(KeyEvent::Hold, 3, 2), G(KeyEvent::Up, 3, 2),
                            S(KeyEvent::Down, Side::Enter), S(KeyEvent::Up, Side::Enter)};
    for (const KeyEvent& e : evs) {
        TEST_ASSERT_EQUAL(1, g.map(e, true, false, true, A));
        TEST_ASSERT_EQUAL(Act::Key, A[0].act);
        TEST_ASSERT_EQUAL(e.type, A[0].key.type);
    }
}

void test_side_keys_pass_through() {
    GestureMapper g;
    TEST_ASSERT_EQUAL(1, g.map(S(KeyEvent::Down, Side::Tab), false, false, false, A));
    TEST_ASSERT_EQUAL(Act::Key, A[0].act);
    TEST_ASSERT_EQUAL(Side::Tab, A[0].key.side);
    TEST_ASSERT_EQUAL(-1, A[0].cell);
}

void test_reset_forgets_enter() {
    GestureMapper g;
    g.map(S(KeyEvent::Down, Side::Enter), false, false, false, A);
    g.reset();
    // an Enter release whose press belonged to another view does nothing
    TEST_ASSERT_EQUAL(0, g.map(S(KeyEvent::Up, Side::Enter), false, false, false, A));
}

void test_reset_consumes_held_keys() {
    GestureMapper g;
    g.map(G(KeyEvent::Down, 2, 1), false, false, false, A);
    g.reset();  // the view changed while the key was held
    TEST_ASSERT_EQUAL(0, g.map(G(KeyEvent::Hold, 2, 1), false, false, false, A));
    g.map(G(KeyEvent::Up, 2, 1), false, false, false, A);
    g.map(G(KeyEvent::Down, 2, 1), false, false, false, A);
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Hold, 2, 1), false, false, false, A));
}

void test_reset_keeps_released_keys_fresh() {
    GestureMapper g;
    g.map(G(KeyEvent::Down, 2, 1), false, false, false, A);
    g.map(G(KeyEvent::Up, 2, 1), false, false, false, A);
    g.reset();
    g.map(G(KeyEvent::Down, 2, 1), false, false, false, A);
    TEST_ASSERT_EQUAL(1, g.map(G(KeyEvent::Hold, 2, 1), false, false, false, A));
}

void test_consume_swallows_hold_and_enter_release() {
    GestureMapper g;
    g.map(G(KeyEvent::Down, 4, 2), false, false, false, A);
    g.consume(G(KeyEvent::Down, 4, 2));
    TEST_ASSERT_EQUAL(0, g.map(G(KeyEvent::Hold, 4, 2), false, false, false, A));
    g.map(S(KeyEvent::Down, Side::Enter), false, false, false, A);
    g.consume(S(KeyEvent::Down, Side::Enter));
    TEST_ASSERT_EQUAL(0, g.map(S(KeyEvent::Up, Side::Enter), false, false, false, A));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mapping);
    RUN_TEST(test_down_up);
    RUN_TEST(test_hold_once);
    RUN_TEST(test_chord);
    RUN_TEST(test_chord_events);
    RUN_TEST(test_tap_selects_only);
    RUN_TEST(test_fn_tap_selects_and_activates);
    RUN_TEST(test_hold_activates);
    RUN_TEST(test_fn_down_then_hold_activates_once);
    RUN_TEST(test_ctrl_hold_does_not_activate);
    RUN_TEST(test_enter_tap_activates_selection);
    RUN_TEST(test_enter_hold_sweeps);
    RUN_TEST(test_raw_mode_passes_everything);
    RUN_TEST(test_side_keys_pass_through);
    RUN_TEST(test_reset_forgets_enter);
    RUN_TEST(test_reset_consumes_held_keys);
    RUN_TEST(test_reset_keeps_released_keys_fresh);
    RUN_TEST(test_consume_swallows_hold_and_enter_release);
    return UNITY_END();
}
