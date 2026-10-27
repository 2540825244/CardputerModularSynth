#include <unity.h>
#include <cstring>
#include <map>
#include "core/modules/keys.h"
#include "core/modules/lfo.h"
#include "core/modules/osc.h"
#include "core/modules/out.h"
#include "core/patch_io.h"
#include "core/registry.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"
#include "ui/controller.h"
#include "ui/layout.h"

void setUp() {}
void tearDown() {}

struct MemStorage : Storage {
    std::map<std::string, std::string> files;
    bool avail = true;
    int writes = 0;
    bool available() override { return avail; }
    bool read(const char* p, std::string& out) override {
        auto it = files.find(p);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool write(const char* p, const std::string& d) override {
        if (!avail) return false;
        writes++;
        files[p] = d;
        return true;
    }
    bool exists(const char* p) override { return files.count(p) > 0; }
};

static uint32_t bigHeap() { return 1000000; }

// Patch and UndoStack are large: keep them off the stack.
static Patch gPatch;
static UndoStack gUndo;
static MemStorage gStore;

enum { TKeys, TOsc, TVcf, TVca, TEnv, TLfo, TDly, TOut };

struct Fx {
    Engine eng{bigHeap};
    UiController ui{gPatch, eng, gUndo, gStore};
    uint32_t t = 0;
    Fx() {
        gPatch = Patch();
        gUndo = UndoStack();
        gStore.files.clear();
        gStore.avail = true;
        gStore.writes = 0;
    }
    void send(KeyEvent::Type ty, bool grid, int c, int r, Side s) {
        KeyEvent e{};
        e.type = ty;
        e.grid = grid;
        e.col = (uint8_t)c;
        e.row = (uint8_t)r;
        e.side = s;
        ui.handle(e, t += 10);
    }
    void sideDown(Side s) { send(KeyEvent::Down, false, 0, 0, s); }
    void sideUp(Side s) { send(KeyEvent::Up, false, 0, 0, s); }
    void side(Side s) { sideDown(s); sideUp(s); }
    void down(int c, int r) { send(KeyEvent::Down, true, c, r, Side::Esc); }
    void up(int c, int r) { send(KeyEvent::Up, true, c, r, Side::Esc); }
    void tap(int c, int r) { down(c, r); up(c, r); }
    void hold(int c, int r) { down(c, r); send(KeyEvent::Hold, true, c, r, Side::Esc); }
    // place type `ty` at (c,r) on the current page via fn+cell
    void place(int ty, int c, int r) {
        sideDown(Side::Fn);
        tap(c, r);
        sideUp(Side::Fn);
        tap(ty, 1);
        side(Side::Enter);
    }
};

void test_place_via_fn() {
    Fx f;
    f.sideDown(Side::Fn);
    f.down(2, 1);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(14, f.ui.state().placeCell);
    f.tap(TOsc, 1);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(TOsc, f.ui.state().placeType);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    const ModuleRec* m = gPatch.findAt(0, 14);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_TRUE(m->type == &kOscType);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_EQUAL(14, f.ui.state().selCell);
}

void test_same_frame_chord() {
    Fx f;
    KeyDecoder dec;
    f.ui.setDecoder(&dec);
    KeyPos pressed[] = {{0, 2}, {3, 1}};  // fn + grid (2,1) in one frame
    KeyEvent ev[16];
    int n = 0;
    dec.update(pressed, 2, 10, ev, &n, 16);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_TRUE(ev[0].grid);  // grid Down arrives before the Fn Down
    for (int i = 0; i < n; i++) f.ui.handle(ev[i], 10);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(14, f.ui.state().placeCell);
}

void test_place_details_back() {
    Fx f;
    f.sideDown(Side::Fn);
    f.down(0, 0);
    f.tap(TOsc, 1);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(-1, f.ui.state().placeType);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_place_via_hold() {
    Fx f;
    f.hold(3, 2);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(27, f.ui.state().placeCell);
}

void test_place_refused() {
    Fx f;
    f.place(TOut, 0, 0);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    f.sideDown(Side::Fn);
    f.tap(1, 0);
    f.sideUp(Side::Fn);
    f.tap(TOut, 1);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL_STRING("MAX 1", f.ui.state().toast);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(TOut, f.ui.state().placeType);
}

void test_select_open_back() {
    Fx f;
    f.place(TOsc, 4, 2);
    f.tap(4, 2);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    TEST_ASSERT_EQUAL(gPatch.findAt(0, 28)->id, f.ui.state().openId);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_keys_opens_play() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::KeysPlay, f.ui.state().view);
}

void test_delete_and_undo() {
    Fx f;
    f.place(TOsc, 1, 1);
    const uint16_t id = gPatch.findAt(0, 13)->id;
    f.side(Side::Del);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    TEST_ASSERT_NULL(f.eng.instance(id));
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(id, gPatch.moduleAt(0).id);
    TEST_ASSERT_NOT_NULL(f.eng.instance(id));
}

void test_move() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.sideDown(Side::Ctrl);
    f.tap(1, 1);
    TEST_ASSERT_EQUAL(gPatch.findAt(0, 13)->id, f.ui.state().moveFromId);
    f.tap(5, 3);
    f.sideUp(Side::Ctrl);
    TEST_ASSERT_NULL(gPatch.findAt(0, 13));
    TEST_ASSERT_NOT_NULL(gPatch.findAt(0, 41));
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
    f.side(Side::Backslash);
    TEST_ASSERT_NOT_NULL(gPatch.findAt(0, 13));
    TEST_ASSERT_NULL(gPatch.findAt(0, 41));
}

void test_move_cancel_by_esc() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.sideDown(Side::Ctrl);
    f.tap(1, 1);
    f.sideUp(Side::Ctrl);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_paging() {
    Fx f;
    f.side(Side::Tab);
    f.side(Side::Tab);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(0, f.ui.state().page);
    f.sideDown(Side::Fn);
    f.side(Side::Tab);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(2, f.ui.state().page);
    f.side(Side::Tab);  // -> 0
    f.side(Side::Tab);  // -> 1
    f.place(TOsc, 2, 2);
    TEST_ASSERT_NOT_NULL(gPatch.findAt(1, 26));
    TEST_ASSERT_NULL(gPatch.findAt(0, 26));
    f.side(Side::Tab);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(0, f.ui.state().page);
    TEST_ASSERT_EQUAL(-1, f.ui.state().selCell);
}

void test_audition() {
    Fx f;
    f.place(TKeys, 0, 0);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.sideDown(Side::Space);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));
    f.sideUp(Side::Space);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
}

void test_toast_expiry() {
    Fx f;
    f.ui.toast("HI", 1000);
    f.ui.tick(2499);
    TEST_ASSERT_EQUAL_STRING("HI", f.ui.state().toast);
    f.ui.tick(2500);
    TEST_ASSERT_EQUAL_STRING("", f.ui.state().toast);
}

static void bump(Patch& p, void*) { p.nextId = (uint16_t)(p.nextId + 1); }

void test_edit_coalescing_breaks() {
    Fx f;
    f.ui.edit(bump, nullptr, 7);
    f.ui.edit(bump, nullptr, 7);
    TEST_ASSERT_EQUAL(1, gUndo.size());
    f.tap(0, 0);  // any non-edit key ends the run
    f.ui.edit(bump, nullptr, 7);
    TEST_ASSERT_EQUAL(2, gUndo.size());
}

// ---- Task 15: module view ----
static bool is(CellRef r, CellKind k, int idx = 0) { return r.kind == k && (k == CellKind::Empty || k == CellKind::Drawing || r.index == idx); }

void test_layout_vcf() {
    TEST_ASSERT_TRUE(is(moduleCell(kVcfType, 1, 0), CellKind::Param, 0));
    TEST_ASSERT_TRUE(is(moduleCell(kVcfType, 1, 3), CellKind::Param, 3));
    TEST_ASSERT_TRUE(is(moduleCell(kVcfType, 2, 2), CellKind::In, 2));
    TEST_ASSERT_TRUE(is(moduleCell(kVcfType, 3, 0), CellKind::Out, 0));
    TEST_ASSERT_TRUE(is(moduleCell(kVcfType, 1, 6), CellKind::Drawing));
    TEST_ASSERT_TRUE(is(moduleCell(kVcfType, 3, 4), CellKind::Empty));
}

void test_layout_vca_no_drawing() {
    TEST_ASSERT_TRUE(is(moduleCell(kVcaType, 1, 6), CellKind::Empty));
}

// Places `ty` at cell (c,r) on page 0, opens it, returns its id.
static uint16_t openModule(Fx& f, int ty, int c, int r) {
    f.place(ty, c, r);
    uint16_t id = gPatch.findAt(0, (uint8_t)(r * 12 + c))->id;
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    return id;
}

void test_coarse_edit() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    f.tap(0, 1);  // CUT
    f.tap(9, 0);  // key 0 -> 100%
    TEST_ASSERT_EQUAL_FLOAT(20000.f, gPatch.find(id)->params[0]);
    TEST_ASSERT_EQUAL_FLOAT(20000.f, f.eng.instance(id)->param(0));
    f.tap(11, 0);  // = -> default
    TEST_ASSERT_EQUAL_FLOAT(1200.f, gPatch.find(id)->params[0]);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, f.eng.instance(id)->param(0));
}

void test_fine_and_undo_coalesce() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    f.tap(1, 1);  // RES
    for (int i = 0; i < 5; i++) f.side(Side::Space);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f + 5.f / 128.f, gPatch.find(id)->params[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f + 5.f / 128.f, f.eng.instance(id)->param(1));
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, gPatch.find(id)->params[1]);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, f.eng.instance(id)->param(1));
}

void test_fine_minus_and_cursor_breaks() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    f.tap(1, 1);
    f.side(Side::Space);
    f.tap(3, 1);  // cursor move ends the run
    f.tap(1, 1);
    f.side(Side::Ctrl);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f, gPatch.find(id)->params[1]);
    f.side(Side::Backslash);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f + 1.f / 128.f, gPatch.find(id)->params[1]);
}

void test_enter_cycles_enum() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    f.tap(2, 1);  // MOD
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL_FLOAT(1.f, gPatch.find(id)->params[2]);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL_FLOAT(0.f, gPatch.find(id)->params[2]);
}

void test_amount_edit() {
    Fx f;
    uint16_t id = openModule(f, TOsc, 0, 0);
    f.tap(0, 3);  // OUT 0
    f.tap(10, 0);  // -
    TEST_ASSERT_EQUAL_FLOAT(-1.f, gPatch.find(id)->outAmount[0]);
    TEST_ASSERT_EQUAL_FLOAT(-1.f, f.eng.instance(id)->outAmount(0));
    f.tap(4, 0);  // 5 -> 50% of -1..1
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.f, gPatch.find(id)->outAmount[0]);
}

void test_del_resets_and_unpatches() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    f.tap(0, 1);
    f.tap(9, 0);
    f.side(Side::Del);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, gPatch.find(id)->params[0]);
    uint16_t other = 0;
    gPatch.addModule(kOscType, 0, 30, &other);
    TEST_ASSERT_EQUAL(PatchError::None, gPatch.addCable(other, 0, id, 0));
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    f.tap(0, 2);  // In 0
    f.side(Side::Del);
    TEST_ASSERT_EQUAL(0, gPatch.cableCount());
    // Del on an Out pin also unpatches (and keeps its amount)
    TEST_ASSERT_EQUAL(PatchError::None, gPatch.addCable(id, 0, other, 0));
    f.tap(0, 3);
    f.side(Side::Del);
    TEST_ASSERT_EQUAL(0, gPatch.cableCount());
}

void test_tab_and_esc_in_module_view() {
    Fx f;
    openModule(f, TVcf, 0, 0);
    f.tap(2, 3);  // an empty cell is selectable but Tab walks Param/Out cells
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(12, f.ui.state().modCell);  // first param
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(13, f.ui.state().modCell);
    f.tap(3, 1);
    f.side(Side::Tab);  // after last param -> the Out
    TEST_ASSERT_EQUAL(36, f.ui.state().modCell);
    f.side(Side::Tab);  // wraps
    TEST_ASSERT_EQUAL(12, f.ui.state().modCell);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_move_across_pages() {
    Fx f;
    f.place(TOsc, 1, 1);
    const uint16_t id = gPatch.findAt(0, 13)->id;
    f.sideDown(Side::Ctrl);
    f.tap(1, 1);
    f.side(Side::Tab);
    f.tap(4, 0);
    f.sideUp(Side::Ctrl);
    TEST_ASSERT_NULL(gPatch.findAt(0, 13));
    TEST_ASSERT_NOT_NULL(gPatch.findAt(1, 4));
    TEST_ASSERT_EQUAL(id, gPatch.findAt(1, 4)->id);
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
}

void test_del_clears_move() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.sideDown(Side::Ctrl);
    f.tap(1, 1);
    f.sideUp(Side::Ctrl);
    TEST_ASSERT_TRUE(f.ui.state().moveFromId >= 0);
    f.side(Side::Del);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
}

void test_audition_release_after_view_change() {
    Fx f;
    f.place(TKeys, 0, 0);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.sideDown(Side::Space);
    f.side(Side::Esc);  // File view while Space is held
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
    f.sideUp(Side::Space);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
}

void test_audition_release_on_view_change() {
    Fx f;
    f.place(TKeys, 0, 0);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.tap(0, 0);
    f.sideDown(Side::Space);
    f.side(Side::Enter);  // KeysPlay opens while Space is held: gate drops at once
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
}

// ---- Task 16: carry ----
static void fnTap(Fx& f, int c, int r) {
    f.sideDown(Side::Fn);
    f.tap(c, r);
    f.sideUp(Side::Fn);
}

void test_carry_lfo_to_vcf() {
    Fx f;
    f.place(TLfo, 0, 0);
    f.place(TVcf, 1, 0);
    const uint16_t lfo = gPatch.findAt(0, 0)->id, vcf = gPatch.findAt(0, 1)->id;
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(lfo, f.ui.state().carry.id);
    TEST_ASSERT_TRUE(f.ui.state().carry.fromOutput);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.tap(1, 0);
    f.side(Side::Enter);
    fnTap(f, 1, 2);
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    const CableRec& c = gPatch.cableAt(0);
    TEST_ASSERT_EQUAL(lfo, c.fromId);
    TEST_ASSERT_EQUAL(0, c.fromOut);
    TEST_ASSERT_EQUAL(vcf, c.toId);
    TEST_ASSERT_EQUAL(1, c.toIn);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
}

void test_carry_from_input() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.place(TVcf, 1, 0);
    const uint16_t osc = gPatch.findAt(0, 0)->id, vcf = gPatch.findAt(0, 1)->id;
    f.tap(1, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 2);  // VCF.in
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    TEST_ASSERT_FALSE(f.ui.state().carry.fromOutput);
    f.side(Side::Esc);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);  // OSC.out
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    TEST_ASSERT_EQUAL(osc, gPatch.cableAt(0).fromId);
    TEST_ASSERT_EQUAL(vcf, gPatch.cableAt(0).toId);
    TEST_ASSERT_EQUAL(0, gPatch.cableAt(0).toIn);
}

void test_carry_same_kind_rejected() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.place(TLfo, 1, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    f.side(Side::Esc);
    f.tap(1, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    TEST_ASSERT_EQUAL_STRING("NEED INPUT", f.ui.state().toast);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(0, gPatch.cableCount());
}

void test_cancel_double_esc() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    f.side(Side::Esc);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    f.side(Side::Esc);  // now a normal Esc again
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
}

void test_esc_not_consecutive_keeps_carry() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    f.side(Side::Esc);
    f.tap(5, 3);  // another key in between
    f.side(Side::Esc);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
}

void test_del_cancels_carry() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    f.side(Side::Esc);
    f.tap(0, 0);
    f.side(Side::Del);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
}

void test_carry_source_undone() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    f.side(Side::Esc);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    // a later pin press starts a fresh carry
    f.place(TVcf, 2, 0);
    f.tap(2, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 2);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(0, gPatch.cableCount());
}

void test_open_module_undone() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    (void)id;
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_carry_duplicate_refused() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.place(TVcf, 1, 0);
    uint16_t osc = gPatch.findAt(0, 0)->id, vcf = gPatch.findAt(0, 1)->id;
    gPatch.addCable(osc, 0, vcf, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);
    f.side(Side::Esc);
    f.tap(1, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 2);
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    TEST_ASSERT_EQUAL_STRING("ALREADY PATCHED", f.ui.state().toast);
}

void test_cable_edit_reaches_engine() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.place(TOut, 1, 0);
    float buf[kBlock];
    auto peak = [&]() {
        float p = 0.f;
        for (int b = 0; b < 20; b++) {
            f.eng.processBlock(buf);
            for (int i = 0; i < kBlock; i++) p = buf[i] > p ? buf[i] : (-buf[i] > p ? -buf[i] : p);
        }
        return p;
    };
    TEST_ASSERT_EQUAL_FLOAT(0.f, peak());
    f.tap(0, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 3);  // OSC.out
    f.side(Side::Esc);
    f.tap(1, 0);
    f.side(Side::Enter);
    fnTap(f, 0, 2);  // OUT.in
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    TEST_ASSERT_TRUE(peak() > 0.01f);
}


// ---- Task 17: keys view, file menu, autosave ----
static void openKeysPlay(Fx& f) {
    f.place(TKeys, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
}

void test_play_notes() {
    Fx f;
    openKeysPlay(f);
    TEST_ASSERT_EQUAL(View::KeysPlay, f.ui.state().view);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.down(0, 3);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(48.f, k->monitor(0));
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));
    f.down(4, 2);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(64.f, k->monitor(0));
    f.up(4, 2);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(48.f, k->monitor(0));
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));
    f.up(0, 3);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
    TEST_ASSERT_EQUAL(64, f.ui.lastNote);
}

void test_play_esc_notes_off() {
    Fx f;
    openKeysPlay(f);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.down(0, 1);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
}

void test_enter_does_not_play() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.tap(0, 0);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.side(Side::Enter);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
}

void test_octave_cells() {
    Fx f;
    openKeysPlay(f);
    const ModuleRec* m = gPatch.findAt(0, 0);
    f.tap(1, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->params[0]);
    f.tap(0, 0);
    f.tap(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(-1.f, m->params[0]);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL_FLOAT(0.f, m->params[0]);
}

void test_keys_flip() {
    Fx f;
    openKeysPlay(f);
    f.tap(11, 0);
    TEST_ASSERT_EQUAL(View::KeysSettings, f.ui.state().view);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::KeysPlay, f.ui.state().view);
    f.tap(11, 0);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

static void toFileSave(Fx& f) {
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
    f.side(Side::Tab);
    TEST_ASSERT_TRUE(f.ui.state().fileSaveMode);
}

void test_save_load_slot() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.place(TOsc, 1, 0);
    toFileSave(f);
    f.tap(2, 0);
    TEST_ASSERT_EQUAL_STRING("SAVED", f.ui.state().toast);
    TEST_ASSERT_EQUAL(1, (int)gStore.files.count("/modsynth/patches/slot03.json"));
    gPatch.clear();
    f.eng.apply(gPatch);
    f.side(Side::Tab);
    f.tap(2, 0);
    TEST_ASSERT_EQUAL_STRING("LOADED", f.ui.state().toast);
    TEST_ASSERT_EQUAL(2, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);  // a load returns to the patch view
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_load_errors() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.side(Side::Esc);
    f.tap(5, 0);
    TEST_ASSERT_EQUAL_STRING("NO FILE", f.ui.state().toast);
    gStore.files["/modsynth/patches/slot01.json"] = "garbage{";
    f.tap(0, 0);
    TEST_ASSERT_EQUAL_STRING("BAD FILE", f.ui.state().toast);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
}

void test_load_empty_patch_file() {
    Fx f;
    f.place(TKeys, 0, 0);
    Patch empty;
    gStore.files["/modsynth/patches/slot02.json"] = patchToJson(empty);
    f.side(Side::Esc);
    f.tap(1, 0);
    TEST_ASSERT_EQUAL_STRING("LOADED", f.ui.state().toast);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_load_validates_open_view() {
    Fx f;
    Patch empty;
    gStore.files["/modsynth/patches/slot02.json"] = patchToJson(empty);
    f.place(TKeys, 0, 0);
    f.tap(0, 0);
    f.side(Side::Esc);
    f.tap(1, 0);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_sd_missing() {
    Fx f;
    f.place(TKeys, 0, 0);
    gStore.avail = false;
    toFileSave(f);
    f.tap(0, 0);
    TEST_ASSERT_EQUAL_STRING("NO SD", f.ui.state().toast);
    // autosave: toast once, then retry only after 30 s
    f.ui.tick(100000);
    TEST_ASSERT_EQUAL_STRING("NO SD", f.ui.state().toast);
    f.ui.tick(101500);
    TEST_ASSERT_EQUAL_STRING("", f.ui.state().toast);
    f.ui.tick(129999);
    TEST_ASSERT_EQUAL_STRING("", f.ui.state().toast);
    f.ui.tick(130000);  // retry fails silently
    TEST_ASSERT_EQUAL_STRING("", f.ui.state().toast);
    TEST_ASSERT_TRUE(f.ui.state().dirty);
    gStore.avail = true;
    f.ui.tick(159999);
    TEST_ASSERT_TRUE(f.ui.state().dirty);
    f.ui.tick(160000);
    TEST_ASSERT_FALSE(f.ui.state().dirty);
}

void test_autosave_debounce() {
    Fx f;
    f.place(TKeys, 0, 0);  // last edit at t
    const uint32_t t0 = f.ui.state().lastEditMs;
    f.ui.tick(t0 + 1999);
    TEST_ASSERT_EQUAL(0, gStore.writes);
    TEST_ASSERT_TRUE(f.ui.state().dirty);
    f.ui.tick(t0 + 2000);
    TEST_ASSERT_EQUAL(1, gStore.writes);
    TEST_ASSERT_EQUAL(1, (int)gStore.files.count("/modsynth/patches/autosave.json"));
    TEST_ASSERT_FALSE(f.ui.state().dirty);
    f.ui.tick(t0 + 5000);
    TEST_ASSERT_EQUAL(1, gStore.writes);
}

void test_boot_load_autosave() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.ui.tick(f.ui.state().lastEditMs + 2000);
    gPatch.clear();
    f.eng.apply(gPatch);
    TEST_ASSERT_TRUE(f.ui.loadAutosave());
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    TEST_ASSERT_FALSE(f.ui.state().dirty);
}

void test_load_resets_ui_state() {
    Fx f;
    f.place(TOsc, 0, 0);
    Patch other;
    uint16_t id = 0;
    other.addModule(kKeysType, 0, 0, &id);
    TEST_ASSERT_EQUAL(gPatch.findAt(0, 0)->id, id);  // same id, different type
    gStore.files["/modsynth/patches/slot01.json"] = patchToJson(other);
    f.tap(0, 0);
    f.side(Side::Enter);
    f.sideDown(Side::Fn);
    f.tap(0, 3);  // OSC output pin: start a carry
    f.sideUp(Side::Fn);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.side(Side::Esc);
    f.tap(5, 2);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.tap(0, 0);
    TEST_ASSERT_EQUAL_STRING("LOADED", f.ui.state().toast);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_EQUAL(-1, f.ui.state().selCell);
    TEST_ASSERT_EQUAL(-1, f.ui.state().modCell);
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
}

// ---- Task 21: hold-and-tilt ----
static void openVcf(Fx& f) {
    f.place(TVcf, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
}

void test_tilt_edit() {
    Fx f;
    openVcf(f);
    const ModuleRec* m = gPatch.findAt(0, 0);
    const ParamSpec& sp = m->type->params[0];
    const float start = paramToNorm(sp, m->params[0]);
    f.hold(0, 1);  // CUT
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(paramFromNorm(sp, start + 0.5f), m->params[0]);
    f.ui.tilt(-200.f);
    TEST_ASSERT_EQUAL_FLOAT(20.f, m->params[0]);
    f.up(0, 1);
    f.ui.tilt(10.f);  // after release: ignored
    TEST_ASSERT_EQUAL_FLOAT(20.f, m->params[0]);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
}

void test_tilt_ignores_pins_and_patch_view() {
    Fx f;
    openVcf(f);
    const ModuleRec* m = gPatch.findAt(0, 0);
    f.hold(0, 2);  // IN pin
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    f.up(0, 2);
    f.side(Side::Esc);
    f.hold(0, 0);
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_place_via_fn);
    RUN_TEST(test_same_frame_chord);
    RUN_TEST(test_place_details_back);
    RUN_TEST(test_place_via_hold);
    RUN_TEST(test_place_refused);
    RUN_TEST(test_select_open_back);
    RUN_TEST(test_keys_opens_play);
    RUN_TEST(test_delete_and_undo);
    RUN_TEST(test_move);
    RUN_TEST(test_move_cancel_by_esc);
    RUN_TEST(test_paging);
    RUN_TEST(test_audition);
    RUN_TEST(test_toast_expiry);
    RUN_TEST(test_edit_coalescing_breaks);
    RUN_TEST(test_layout_vcf);
    RUN_TEST(test_layout_vca_no_drawing);
    RUN_TEST(test_coarse_edit);
    RUN_TEST(test_fine_and_undo_coalesce);
    RUN_TEST(test_fine_minus_and_cursor_breaks);
    RUN_TEST(test_enter_cycles_enum);
    RUN_TEST(test_amount_edit);
    RUN_TEST(test_del_resets_and_unpatches);
    RUN_TEST(test_tab_and_esc_in_module_view);
    RUN_TEST(test_move_across_pages);
    RUN_TEST(test_del_clears_move);
    RUN_TEST(test_audition_release_after_view_change);
    RUN_TEST(test_audition_release_on_view_change);
    RUN_TEST(test_carry_lfo_to_vcf);
    RUN_TEST(test_carry_from_input);
    RUN_TEST(test_carry_same_kind_rejected);
    RUN_TEST(test_cancel_double_esc);
    RUN_TEST(test_esc_not_consecutive_keeps_carry);
    RUN_TEST(test_del_cancels_carry);
    RUN_TEST(test_carry_source_undone);
    RUN_TEST(test_open_module_undone);
    RUN_TEST(test_carry_duplicate_refused);
    RUN_TEST(test_cable_edit_reaches_engine);
    RUN_TEST(test_play_notes);
    RUN_TEST(test_play_esc_notes_off);
    RUN_TEST(test_enter_does_not_play);
    RUN_TEST(test_octave_cells);
    RUN_TEST(test_keys_flip);
    RUN_TEST(test_save_load_slot);
    RUN_TEST(test_load_errors);
    RUN_TEST(test_load_empty_patch_file);
    RUN_TEST(test_load_validates_open_view);
    RUN_TEST(test_sd_missing);
    RUN_TEST(test_autosave_debounce);
    RUN_TEST(test_boot_load_autosave);
    RUN_TEST(test_load_resets_ui_state);
    RUN_TEST(test_tilt_edit);
    RUN_TEST(test_tilt_ignores_pins_and_patch_view);
    return UNITY_END();
}
