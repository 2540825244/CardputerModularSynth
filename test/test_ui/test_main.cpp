#include <unity.h>
#include <cstring>
#include <map>
#include "core/modules/env.h"
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
#include "ui/keys_layout.h"
#include "core/scales.h"
#include "core/modules/mod.h"
#include "core/presets.h"
#include "ui/anim.h"
#include "ui/side_icons.h"

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

static int registryIndex(const ModuleTypeInfo& t) {
    int n = 0;
    const ModuleTypeInfo* const* all = allTypes(&n);
    for (int i = 0; i < n; i++)
        if (all[i] == &t) return i;
    return -1;
}

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
    void holdSide(Side s) { sideDown(s); send(KeyEvent::Hold, false, 0, 0, s); }
    void slot(int c) { tap(c, 0); side(Side::Enter); }  // file menu: select a slot, then do it
    // place-menu tile of type index `ty` (allTypes order)
    // place-menu tile of type `ty`: flips to its category page with tab first
    void tilePage(int ty, uint8_t* r, uint8_t* c) {
        int pg = 0;
        placeCellOf(ty, &pg, r, c);
        for (int i = 0; i < 8 && ui.state().placePage != pg; i++) side(Side::Tab);
    }
    void tileTap(int ty) { uint8_t r, c; tilePage(ty, &r, &c); tap(c, r); }
    void tileHold(int ty) { uint8_t r, c; tilePage(ty, &r, &c); hold(c, r); }
    void tileUp(int ty) { uint8_t r, c; tilePage(ty, &r, &c); up(c, r); }
    // place type `ty` at (c,r) on the current page via fn+cell
    void place(int ty, int c, int r) {
        sideDown(Side::Fn);
        tap(c, r);
        sideUp(Side::Fn);
        tileTap(ty);
        side(Side::Enter);
    }
};

void test_place_via_fn() {
    Fx f;
    f.sideDown(Side::Fn);
    f.down(2, 1);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    TEST_ASSERT_EQUAL(14, f.ui.state().placeCell);
    f.sideUp(Side::Fn);  // fn still held on the tile would place it at once (do-it)
    f.tileTap(TOsc);
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
    f.sideUp(Side::Fn);
    f.tileTap(TOsc);
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
    f.tileTap(TOut);
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
    TEST_ASSERT_TRUE(f.ui.onPlayPage());
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
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL_FLOAT(3.f, gPatch.find(id)->params[2]);  // LP HP BP NOT
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
    f.tap(1, 1);
    f.side(Side::Tab);  // VCF fits one page: tab keeps the page and the cursor
    TEST_ASSERT_EQUAL(0, f.ui.state().modPage);
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
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.side(Side::Esc);  // v1.2: in the patch view one esc drops the cable (its icon is cancel)
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
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
    TEST_ASSERT_TRUE(f.ui.onPlayPage());
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.down(0, 3);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(36.f, k->monitor(0));
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));
    f.down(4, 2);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(52.f, k->monitor(0));
    f.up(4, 2);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(36.f, k->monitor(0));
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));
    f.up(0, 3);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
    TEST_ASSERT_EQUAL(52, f.ui.lastNote);
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
    f.side(Side::Space);
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->params[0]);
    f.side(Side::Ctrl);
    f.side(Side::Ctrl);
    TEST_ASSERT_EQUAL_FLOAT(-1.f, m->params[0]);
    f.tap(1, 0);  // the top row plays notes now, it no longer shifts octaves
    TEST_ASSERT_EQUAL_FLOAT(-1.f, m->params[0]);
}

void test_keys_flip() {
    Fx f;
    openKeysPlay(f);
    f.side(Side::Tab);
    TEST_ASSERT_TRUE(f.ui.state().view == View::Module && f.ui.state().modPage == 1);
    f.side(Side::Tab);
    TEST_ASSERT_TRUE(f.ui.onPlayPage());
    f.side(Side::Tab);
    f.side(Side::Esc);  // esc leaves from any page
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

static void toFileSave(Fx& f) {
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(1, f.ui.state().filePage);
}

void test_save_load_slot() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.place(TOsc, 1, 0);
    toFileSave(f);
    f.slot(2);
    TEST_ASSERT_EQUAL_STRING("SAVED", f.ui.state().toast);
    TEST_ASSERT_EQUAL(1, (int)gStore.files.count("/modsynth/patches/slot03.json"));
    gPatch.clear();
    f.eng.apply(gPatch);
    f.sideDown(Side::Fn);
    f.side(Side::Tab);  // back to LOAD (tab goes on to PRESETS)
    f.sideUp(Side::Fn);
    f.slot(2);
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
    f.slot(5);
    TEST_ASSERT_EQUAL_STRING("NO FILE", f.ui.state().toast);
    gStore.files["/modsynth/patches/slot01.json"] = "garbage{";
    f.slot(0);
    TEST_ASSERT_EQUAL_STRING("BAD FILE", f.ui.state().toast);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
}

void test_load_empty_patch_file() {
    Fx f;
    f.place(TKeys, 0, 0);
    Patch empty;
    gStore.files["/modsynth/patches/slot02.json"] = patchToJson(empty);
    f.side(Side::Esc);
    f.slot(1);
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
    f.slot(1);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_sd_missing() {
    Fx f;
    f.place(TKeys, 0, 0);
    gStore.avail = false;
    toFileSave(f);
    f.slot(0);
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
    f.side(Side::Esc);  // drops the carry (v1.2): the file menu can't be reached while carrying
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::File, f.ui.state().view);
    f.slot(0);
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
    f.tap(0, 1);  // CUT
    f.holdSide(Side::Enter);
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(paramFromNorm(sp, start + 0.5f), m->params[0]);
    f.ui.tilt(-200.f);
    TEST_ASSERT_EQUAL_FLOAT(20.f, m->params[0]);
    f.sideUp(Side::Enter);
    f.ui.tilt(10.f);  // after release: ignored
    TEST_ASSERT_EQUAL_FLOAT(20.f, m->params[0]);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
}

void test_hold_enter_on_pin_carries_not_sweeps() {
    Fx f;
    openVcf(f);
    const ModuleRec* m = gPatch.findAt(0, 0);
    f.tap(0, 2);  // IN pin
    f.holdSide(Side::Enter);
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.sideUp(Side::Enter);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);  // no second toggle on release
}

void test_tilt_deadband() {
    Fx f;
    openVcf(f);
    const ModuleRec* m = gPatch.findAt(0, 0);
    f.ui.tick(f.ui.state().lastEditMs + 2000);  // flush the placement autosave
    gUndo = UndoStack();
    f.tap(0, 1);
    f.holdSide(Side::Enter);
    f.ui.tilt(0.9f);
    f.ui.tilt(-1.f);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
    TEST_ASSERT_FALSE(f.ui.state().dirty);
    f.ui.tilt(5.f);
    TEST_ASSERT_TRUE(m->params[0] != 1200.f);
    f.ui.tilt(0.5f);  // once moved, small values still apply
    f.sideUp(Side::Enter);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
}

void test_no_autosave_mid_tilt() {
    Fx f;
    openVcf(f);
    f.tap(0, 1);
    f.holdSide(Side::Enter);
    f.ui.tilt(20.f);
    const uint32_t t0 = f.ui.state().lastEditMs;
    f.ui.tick(t0 + 5000);
    TEST_ASSERT_EQUAL(0, gStore.writes);
    TEST_ASSERT_TRUE(f.ui.state().dirty);
    f.sideUp(Side::Enter);  // the debounce restarts at the release
    f.ui.tick(f.ui.state().lastEditMs + 1999);
    TEST_ASSERT_EQUAL(0, gStore.writes);
    f.ui.tick(f.ui.state().lastEditMs + 2000);
    TEST_ASSERT_EQUAL(1, gStore.writes);
}


// ---- v1.1: one do-it gesture ----
static const ModuleRec* placeAndOpen(Fx& f, int ty) {
    f.place(ty, 0, 0);
    f.tap(0, 0);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    return gPatch.findAt(0, 0);
}

void test_hold_module_opens_it() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.hold(0, 0);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    TEST_ASSERT_EQUAL(gPatch.findAt(0, 0)->id, f.ui.state().openId);
}

void test_fn_tap_module_opens_it() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.sideDown(Side::Fn);
    f.tap(0, 0);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    TEST_ASSERT_EQUAL(-1, f.ui.state().modCell);
}

void test_hold_enter_in_patch_view_opens_once() {
    Fx f;
    f.place(TOsc, 0, 0);
    f.tap(0, 0);
    f.holdSide(Side::Enter);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    f.sideUp(Side::Enter);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    TEST_ASSERT_EQUAL(-1, f.ui.state().modCell);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
}

void test_place_by_hold_tile() {
    Fx f;
    f.sideDown(Side::Fn);
    f.tap(2, 1);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(View::Place, f.ui.state().view);
    f.tileHold(TOsc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    const ModuleRec* m = gPatch.findAt(0, 14);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_TRUE(m->type == &kOscType);
    f.tileUp(TOsc);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
}

void test_place_by_fn_tile() {
    Fx f;
    f.sideDown(Side::Fn);
    f.tap(2, 1);
    f.tileTap(TOsc);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    TEST_ASSERT_TRUE(f.ui.undo());
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_hold_pin_picks_up_cable() {
    Fx f;
    placeAndOpen(f, TOsc);
    f.hold(0, 3);  // OUT pin
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    TEST_ASSERT_TRUE(f.ui.state().carry.fromOutput);
}

void test_enter_on_pin_picks_up_cable() {
    Fx f;
    placeAndOpen(f, TOsc);
    f.tap(0, 3);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);  // a tap only selects
    f.side(Side::Enter);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
}

void test_hold_stepped_param_cycles() {
    Fx f;
    const ModuleRec* m = placeAndOpen(f, TOsc);
    TEST_ASSERT_EQUAL_FLOAT(2.f, m->params[0]);  // wave default
    f.hold(0, 1);
    TEST_ASSERT_EQUAL_FLOAT(3.f, m->params[0]);
    f.up(0, 1);
    f.hold(0, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.f, m->params[0]);  // wraps
    TEST_ASSERT_FALSE(f.ui.tiltActive());
}

void test_hold_plain_param_does_nothing() {
    Fx f;
    const ModuleRec* m = placeAndOpen(f, TVcf);
    f.hold(0, 1);  // CUT
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    TEST_ASSERT_EQUAL(1, f.ui.state().modCell / kGridCols);
}

void test_file_tap_selects_then_enter_saves() {
    Fx f;
    f.place(TKeys, 0, 0);
    toFileSave(f);
    TEST_ASSERT_EQUAL(-1, f.ui.state().fileSlot);
    f.tap(2, 0);
    TEST_ASSERT_EQUAL(0, (int)gStore.files.count("/modsynth/patches/slot03.json"));
    TEST_ASSERT_EQUAL(2, f.ui.state().fileSlot);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL_STRING("SAVED", f.ui.state().toast);
    TEST_ASSERT_EQUAL(1, (int)gStore.files.count("/modsynth/patches/slot03.json"));
}

void test_file_hold_slot_loads() {
    Fx f;
    f.place(TKeys, 0, 0);
    toFileSave(f);
    f.slot(0);
    f.side(Side::Tab);  // back to LOAD
    f.hold(0, 0);
    TEST_ASSERT_EQUAL_STRING("LOADED", f.ui.state().toast);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_file_slot_cleared_on_open() {
    Fx f;
    f.side(Side::Esc);
    f.tap(4, 0);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    f.side(Side::Esc);
    TEST_ASSERT_EQUAL(-1, f.ui.state().fileSlot);
}

void test_keys_settings_enter_is_do_it() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.side(Side::Enter);
    TEST_ASSERT_TRUE(f.ui.onPlayPage());
    f.side(Side::Tab);
    TEST_ASSERT_TRUE(f.ui.state().view == View::Module && f.ui.state().modPage == 1);
    f.side(Side::Enter);  // enter is do-it, not a flip
    TEST_ASSERT_TRUE(f.ui.state().view == View::Module && f.ui.state().modPage == 1);
}

// ---- v1.2: place menu, one page per category ----
void test_place_pages_by_category() {
    TEST_ASSERT_EQUAL(5, placePageCount());
    TEST_ASSERT_EQUAL(TKeys, placeTypeAt(0, 1, 0));
    TEST_ASSERT_EQUAL(TOsc, placeTypeAt(0, 1, 1));
    TEST_ASSERT_EQUAL(TEnv, placeTypeAt(1, 1, 0));
    TEST_ASSERT_EQUAL(TLfo, placeTypeAt(1, 1, 1));
    TEST_ASSERT_EQUAL(TVcf, placeTypeAt(2, 1, 0));
    TEST_ASSERT_EQUAL(TDly, placeTypeAt(3, 1, 0));
    TEST_ASSERT_EQUAL(TOut, placeTypeAt(4, 1, 0));
    TEST_ASSERT_EQUAL(-1, placeTypeAt(0, 0, 0));
    TEST_ASSERT_EQUAL(-1, placeTypeAt(0, 4, 0));
    TEST_ASSERT_EQUAL(-1, placeTypeAt(5, 1, 0));
}

void test_place_cell_of_inverts() {
    int n = 0;
    allTypes(&n);
    for (int i = 0; i < n; i++) {
        int pg = 0;
        uint8_t r = 0, c = 0;
        TEST_ASSERT_TRUE(placeCellOf(i, &pg, &r, &c));
        TEST_ASSERT_EQUAL(i, placeTypeAt(pg, r, c));
    }
    int pg;
    uint8_t r, c;
    TEST_ASSERT_FALSE(placeCellOf(n, &pg, &r, &c));
    TEST_ASSERT_FALSE(placeCellOf(-1, &pg, &r, &c));
}

void test_place_tab_flips_category() {
    Fx f;
    f.sideDown(Side::Fn);
    f.tap(2, 1);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(0, f.ui.state().placePage);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(1, f.ui.state().placePage);
    f.tap(0, 1);
    TEST_ASSERT_EQUAL(TEnv, f.ui.state().placeType);
    f.tap(5, 0);  // the top row holds the details: no tile there
    TEST_ASSERT_EQUAL(TEnv, f.ui.state().placeType);
    f.sideDown(Side::Fn);
    f.side(Side::Tab);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(0, f.ui.state().placePage);
    TEST_ASSERT_EQUAL(-1, f.ui.state().placeType);  // a page flip clears the details
    f.side(Side::Tab);
    f.tap(0, 1);
    f.side(Side::Enter);
    const ModuleRec* m = gPatch.findAt(0, 14);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_TRUE(m->type == &kEnvType);
}

void test_all_blurbs_short() {
    int n = 0;
    const ModuleTypeInfo* const* t = allTypes(&n);
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE_MESSAGE(strlen(t[i]->blurb) <= 32, t[i]->id);
        TEST_ASSERT_TRUE_MESSAGE(strlen(t[i]->name) <= 3, t[i]->id);
    }
}

// ---- v1.1 review round 1 ----
void test_carry_out_of_keys_settings() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.side(Side::Enter);
    f.side(Side::Tab);  // settings
    TEST_ASSERT_TRUE(f.ui.state().modPage == 1);
    f.sideDown(Side::Fn);
    f.tap(0, 3);  // PIT out: start a carry
    f.sideUp(Side::Fn);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.side(Side::Esc);  // to the patch view, still carrying
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
}

void test_hold_enter_on_stepped_param_steps() {
    Fx f;
    const ModuleRec* m = placeAndOpen(f, TVcf);
    f.tap(2, 1);  // MOD (LP/HP)
    f.holdSide(Side::Enter);
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->params[2]);
    f.sideUp(Side::Enter);
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->params[2]);
}

void test_file_tab_clears_slot() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.side(Side::Esc);
    f.tap(2, 0);  // picked in LOAD mode
    f.side(Side::Tab);  // to SAVE
    TEST_ASSERT_EQUAL(-1, f.ui.state().fileSlot);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(0, (int)gStore.files.count("/modsynth/patches/slot03.json"));
}

void test_ctrl_hold_pin_still_carries_in_module_view() {
    Fx f;
    placeAndOpen(f, TOsc);
    f.sideDown(Side::Ctrl);
    f.hold(0, 3);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
}

void test_select_during_sweep_ends_it() {
    Fx f;
    const ModuleRec* m = placeAndOpen(f, TVcf);
    f.tap(0, 1);  // CUT
    f.holdSide(Side::Enter);
    TEST_ASSERT_TRUE(f.ui.tiltActive());
    f.tap(1, 1);  // RES
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    f.ui.tilt(30.f);
    TEST_ASSERT_EQUAL_FLOAT(1200.f, m->params[0]);
    f.sideUp(Side::Enter);
    f.tap(0, 1);
    f.holdSide(Side::Enter);
    f.side(Side::Tab);
    TEST_ASSERT_FALSE(f.ui.tiltActive());
}

void test_coarse_tap_during_sweep_ends_it() {
    Fx f;
    const ModuleRec* m = placeAndOpen(f, TVcf);
    f.tap(0, 1);  // CUT
    f.holdSide(Side::Enter);
    f.ui.tilt(10.f);
    f.tap(4, 0);  // coarse 50%
    const float coarse = m->params[0];
    TEST_ASSERT_FALSE(f.ui.tiltActive());
    f.ui.tilt(20.f);  // the device keeps moving
    TEST_ASSERT_EQUAL_FLOAT(coarse, m->params[0]);
}

// ---- value strip fill (owner, 2026-10-28: values between 10% steps must show truthfully) ----
void test_strip_fill_partial_segment() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, stripFill(0.35f, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, stripFill(0.35f, 2));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, stripFill(0.35f, 3));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.f, stripFill(0.35f, 4));
}

void test_strip_fill_ends() {
    for (int c = 0; c < 10; c++) {
        TEST_ASSERT_EQUAL_FLOAT(0.f, stripFill(0.f, c));
        TEST_ASSERT_EQUAL_FLOAT(1.f, stripFill(1.f, c));
    }
    TEST_ASSERT_EQUAL_FLOAT(1.f, stripFill(0.4f, 3));  // exactly on a step: full, no sliver next
    TEST_ASSERT_EQUAL_FLOAT(0.f, stripFill(0.4f, 4));
    TEST_ASSERT_EQUAL_FLOAT(0.f, stripFill(0.5f, 10));  // MIN/DEF cells never fill
}


void test_input_amount_edit() {
    Fx f;
    uint16_t id = openModule(f, TOsc, 0, 0);
    f.tap(1, 2);   // IN 1 (FM)
    f.tap(10, 0);  // - = min
    TEST_ASSERT_EQUAL_FLOAT(-1.f, gPatch.find(id)->inAmount[1]);
    TEST_ASSERT_EQUAL_FLOAT(-1.f, f.eng.instance(id)->inAmount(1));
    f.side(Side::Space);  // fine +
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.f + 2.f / 128.f, gPatch.find(id)->inAmount[1]);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL_FLOAT(1.f, gPatch.find(id)->inAmount[1]);  // fine + coalesced with the coarse set
    TEST_ASSERT_EQUAL_FLOAT(1.f, gPatch.find(id)->inAmount[0]);
}


// ---- v1.2 Task 2: module pages ----
static const char* const kBigIn[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
static ParamSpec kBigParams[14];
static Module* createNone() { return nullptr; }
static const ModuleTypeInfo& bigType(Drawing d) {
    static ModuleTypeInfo t;
    for (int i = 0; i < 14; i++) kBigParams[i] = {"p", "P", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr};
    t = ModuleTypeInfo{"big", "BIG", "test", Category::Source, Icon::Osc, d, 14, kBigParams, 8, kBigIn, kBigIn,
                       1, kBigIn, kBigIn, 10, 100, 0, createNone};
    return t;
}

void test_auto_pages_overflow() {
    const ModuleTypeInfo& t = bigType(Drawing::None);
    TEST_ASSERT_EQUAL(2, modulePageCount(t));
    TEST_ASSERT_TRUE(is(moduleCell(t, 0, 1, 11), CellKind::Param, 11));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 1, 0), CellKind::Param, 12));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 1, 1), CellKind::Param, 13));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 1, 2), CellKind::Empty));
    TEST_ASSERT_TRUE(is(moduleCell(t, 0, 2, 7), CellKind::In, 7));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 2, 0), CellKind::Empty));
    TEST_ASSERT_EQUAL(1, modulePageCount(kVcaType));
}

void test_drawing_page0_width5() {
    const ModuleTypeInfo& t = bigType(Drawing::FilterCurve);
    TEST_ASSERT_EQUAL(2, modulePageCount(t));  // page 0 holds params 0-4, page 1 holds 5-13
    TEST_ASSERT_TRUE(is(moduleCell(t, 0, 1, 5), CellKind::Drawing));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 1, 0), CellKind::Param, 5));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 2, 2), CellKind::In, 7));
    TEST_ASSERT_TRUE(is(moduleCell(t, 1, 1, 9), CellKind::Empty));
    TEST_ASSERT_EQUAL(Drawing::FilterCurve, modulePageDrawing(t, 0));
    TEST_ASSERT_EQUAL(Drawing::None, modulePageDrawing(t, 1));
}

void test_keys_pages() {
    TEST_ASSERT_EQUAL(2, modulePageCount(kKeysType));
    TEST_ASSERT_EQUAL(PageKind::Play, modulePageKind(kKeysType, 0));
    TEST_ASSERT_EQUAL(PageKind::Cells, modulePageKind(kKeysType, 1));
    TEST_ASSERT_EQUAL_STRING("PLAY", modulePageName(kKeysType, 0));
    TEST_ASSERT_TRUE(is(moduleCell(kKeysType, 1, 1, 0), CellKind::Param, 4));  // LAYOUT first
    TEST_ASSERT_TRUE(is(moduleCell(kKeysType, 1, 1, 3), CellKind::Param, 0));  // then OCT
    TEST_ASSERT_TRUE(is(moduleCell(kKeysType, 1, 3, 1), CellKind::Out, 1));
}

void test_fn_tab_previous_page() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.side(Side::Enter);
    f.sideDown(Side::Fn);
    f.side(Side::Tab);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(1, f.ui.state().modPage);  // wraps backwards from page 0
    f.tap(0, 1);
    TEST_ASSERT_EQUAL(12, f.ui.state().modCell);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(0, f.ui.state().modPage);
    TEST_ASSERT_EQUAL(-1, f.ui.state().modCell);  // a page flip clears the cursor
}

void test_tab_flip_releases_notes() {
    Fx f;
    openKeysPlay(f);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.down(0, 2);
    f.side(Side::Tab);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
    f.up(0, 2);
}


// ---- v1.2 Task 3: gestures ----
void test_chord_moves_without_opening() {
    Fx f;
    f.place(TOsc, 1, 1);
    const uint16_t id = gPatch.findAt(0, 13)->id;
    f.down(1, 1);
    f.t += 150;  // a chord, not a two-thumb rollover
    f.down(5, 3);
    f.send(KeyEvent::Hold, true, 1, 1, Side::Esc);
    f.up(5, 3);
    f.up(1, 1);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_NULL(gPatch.findAt(0, 13));
    TEST_ASSERT_EQUAL(id, gPatch.findAt(0, 41)->id);
    TEST_ASSERT_EQUAL(41, f.ui.state().selCell);
}

void test_hold_then_press_opens_only() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.hold(1, 1);
    TEST_ASSERT_EQUAL(View::Module, f.ui.state().view);
    f.down(5, 3);
    f.up(5, 3);
    f.up(1, 1);
    TEST_ASSERT_NOT_NULL(gPatch.findAt(0, 13));
    TEST_ASSERT_NULL(gPatch.findAt(0, 41));
}

void test_chord_swaps() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.place(TVcf, 2, 1);
    const uint16_t o = gPatch.findAt(0, 13)->id, v = gPatch.findAt(0, 14)->id;
    f.down(1, 1);
    f.t += 150;
    f.down(2, 1);
    f.up(2, 1);
    f.up(1, 1);
    TEST_ASSERT_EQUAL(v, gPatch.findAt(0, 13)->id);
    TEST_ASSERT_EQUAL(o, gPatch.findAt(0, 14)->id);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(o, gPatch.findAt(0, 13)->id);
}

void test_ctrl_after_select_moves() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.tap(1, 1);
    f.side(Side::Ctrl);
    TEST_ASSERT_EQUAL(gPatch.findAt(0, 13)->id, f.ui.state().moveFromId);
    f.tap(5, 3);
    TEST_ASSERT_NOT_NULL(gPatch.findAt(0, 41));
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
    f.tap(5, 3);
    f.side(Side::Ctrl);
    f.side(Side::Ctrl);  // a second ctrl tap cancels
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
}

void test_move_onto_module_swaps() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.place(TVcf, 2, 1);
    const uint16_t o = gPatch.findAt(0, 13)->id;
    f.tap(1, 1);
    f.side(Side::Ctrl);
    f.tap(2, 1);
    TEST_ASSERT_EQUAL(o, gPatch.findAt(0, 14)->id);
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
}

void test_fn_target_copies() {
    Fx f;
    f.place(TOsc, 1, 1);
    gPatch.find(gPatch.findAt(0, 13)->id)->params[0] = 1.f;
    f.tap(1, 1);
    f.side(Side::Ctrl);
    f.sideDown(Side::Fn);
    f.tap(6, 3);
    f.sideUp(Side::Fn);
    TEST_ASSERT_NOT_NULL(gPatch.findAt(0, 13));
    const ModuleRec* c = gPatch.findAt(0, 42);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_TRUE(c->type == &kOscType);
    TEST_ASSERT_EQUAL_FLOAT(1.f, c->params[0]);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);  // the fn+tap did not also open it
    TEST_ASSERT_EQUAL(-1, f.ui.state().moveFromId);
}

void test_fn_tap_releases_carry() {
    Fx f;
    uint16_t id = openModule(f, TOsc, 0, 0);
    (void)id;
    f.sideDown(Side::Fn);
    f.tap(0, 3);
    f.sideUp(Side::Fn);
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.side(Side::Esc);  // to the patch view, still carrying
    TEST_ASSERT_TRUE(f.ui.state().carry.active);
    f.side(Side::Fn);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
}

void test_fn_pin_still_drops() {
    Fx f;
    f.place(TVcf, 1, 1);
    uint16_t o = openModule(f, TOsc, 0, 0);
    f.sideDown(Side::Fn);
    f.tap(0, 3);
    f.sideUp(Side::Fn);
    f.side(Side::Esc);
    f.tap(1, 1);
    f.side(Side::Enter);
    f.sideDown(Side::Fn);
    f.tap(0, 2);
    f.sideUp(Side::Fn);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    TEST_ASSERT_EQUAL(o, gPatch.cableAt(0).fromId);
}

void test_chord_pins_patch() {
    Fx f;
    uint16_t id = openModule(f, TVcf, 0, 0);
    f.down(0, 3);  // OUT
    f.t += 150;
    f.down(1, 2);  // CUT in
    f.up(1, 2);
    f.up(0, 3);
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    TEST_ASSERT_EQUAL(id, gPatch.cableAt(0).fromId);
    TEST_ASSERT_EQUAL(1, gPatch.cableAt(0).toIn);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
}

void test_redo_fn_backslash() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    f.sideDown(Side::Fn);
    f.side(Side::Backslash);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_side_held_tracked() {
    Fx f;
    f.sideDown(Side::Del);
    TEST_ASSERT_TRUE(f.ui.state().sideHeld & (1u << (int)Side::Del));
    TEST_ASSERT_EQUAL(f.t, f.ui.state().sideDownMs[(int)Side::Del]);
    f.sideUp(Side::Del);
    TEST_ASSERT_FALSE(f.ui.state().sideHeld & (1u << (int)Side::Del));
    f.down(3, 2);
    TEST_ASSERT_EQUAL(27, f.ui.state().pressCell);
    TEST_ASSERT_EQUAL(f.t, f.ui.state().pressMs);
}


// ---- v1.2 Task 4: side icons ----
static SideIcons icons(Fx& f) { return sideIcons(f.ui.state(), gPatch, f.ui.fnHeld()); }
static const int kEsc = (int)Side::Esc, kTab = (int)Side::Tab, kFnS = (int)Side::Fn, kEnt = (int)Side::Enter,
                 kBs = (int)Side::Backslash, kDel = (int)Side::Del;

void test_icons_move_esc_cancel() {
    Fx f;
    f.place(TOsc, 1, 1);
    TEST_ASSERT_TRUE(icons(f).icon[kEsc] == Ic::File);
    f.side(Side::Ctrl);  // OSC is selected after placing
    TEST_ASSERT_TRUE(f.ui.state().moveFromId >= 0);
    SideIcons r = icons(f);
    TEST_ASSERT_TRUE(r.icon[kEsc] == Ic::Cancel);
    TEST_ASSERT_TRUE(r.icon[kFnS] == Ic::Copy);
    TEST_ASSERT_TRUE(r.active[(int)Side::Ctrl]);
    f.side(Side::Esc);
    TEST_ASSERT_TRUE(icons(f).icon[kEsc] == Ic::File);
}

void test_icons_carry_fn_active() {
    Fx f;
    openModule(f, TOsc, 0, 0);
    f.sideDown(Side::Fn);
    f.tap(0, 3);
    f.sideUp(Side::Fn);
    TEST_ASSERT_TRUE(icons(f).active[kFnS]);
    f.side(Side::Esc);
    SideIcons r = icons(f);
    TEST_ASSERT_TRUE(r.active[kFnS]);
    TEST_ASSERT_TRUE(r.icon[kEsc] == Ic::Cancel);
    f.side(Side::Esc);  // in the patch view one esc drops the cable
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
}

void test_icons_enter_follows_selection() {
    Fx f;
    TEST_ASSERT_FALSE(icons(f).shown[kEnt]);
    f.tap(3, 2);
    TEST_ASSERT_TRUE(icons(f).icon[kEnt] == Ic::Plus);
    TEST_ASSERT_FALSE(icons(f).shown[kDel]);
    openModule(f, TVcf, 0, 0);
    f.tap(0, 1);  // CUT
    TEST_ASSERT_TRUE(icons(f).icon[kEnt] == Ic::Tilt);
    f.tap(2, 1);  // MOD
    TEST_ASSERT_TRUE(icons(f).icon[kEnt] == Ic::Toggle);
    f.tap(0, 2);  // IN
    TEST_ASSERT_TRUE(icons(f).icon[kEnt] == Ic::Cable);
    TEST_ASSERT_TRUE(icons(f).shown[kDel]);
}

void test_icons_tab_page_text() {
    Fx f;
    TEST_ASSERT_EQUAL_STRING("1/3", icons(f).tabText);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL_STRING("2/3", icons(f).tabText);
    f.side(Side::Tab);
    f.side(Side::Tab);  // back to page 1, where openModule looks
    openModule(f, TVca, 0, 0);
    TEST_ASSERT_EQUAL_STRING("", icons(f).tabText);
}

void test_icons_fn_held_redo() {
    Fx f;
    TEST_ASSERT_TRUE(icons(f).icon[kBs] == Ic::Undo);
    f.sideDown(Side::Fn);
    TEST_ASSERT_TRUE(icons(f).icon[kBs] == Ic::Redo);
    f.sideUp(Side::Fn);
}

void test_help_visible_after_fn_hold() {
    Fx f;
    f.sideDown(Side::Fn);
    TEST_ASSERT_FALSE(helpVisible(f.ui.state(), f.t + 100));
    TEST_ASSERT_TRUE(helpVisible(f.ui.state(), f.t + 700));
    f.side(Side::Tab);
    TEST_ASSERT_FALSE(helpVisible(f.ui.state(), f.t + 700));
    f.sideUp(Side::Fn);
    TEST_ASSERT_FALSE(helpVisible(f.ui.state(), f.t + 700));
}

void test_side_pressed_flash() {
    Fx f;
    f.sideDown(Side::Del);
    TEST_ASSERT_TRUE(sidePressed(f.ui.state(), kDel, f.t + 500));
    f.sideUp(Side::Del);
    const uint32_t down = f.ui.state().sideDownMs[kDel];
    TEST_ASSERT_TRUE(sidePressed(f.ui.state(), kDel, down + 100));
    TEST_ASSERT_FALSE(sidePressed(f.ui.state(), kDel, down + 200));
}


// ---- v1.2 Task 6: animation helpers ----
void test_ease_converges() {
    float v = 0.f;
    for (int i = 0; i < 40; i++) v = ease(v, 1.f, 33.f, 60.f);
    TEST_ASSERT_EQUAL_FLOAT(1.f, v);
    TEST_ASSERT_TRUE(ease(0.f, 1.f, 33.f, 60.f) > 0.3f);
    TEST_ASSERT_TRUE(ease(0.f, 1.f, 33.f, 60.f) < 0.6f);
    TEST_ASSERT_EQUAL_FLOAT(0.f, ease(0.f, 1.f, 0.f, 60.f));
}

void test_zoom_t_bounds() {
    TEST_ASSERT_EQUAL_FLOAT(0.f, zoomT(1000, 1000));
    TEST_ASSERT_EQUAL_FLOAT(1.f, zoomT(1000, 1000 + kZoomMs));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, zoomT(1000, 1000 + kZoomMs / 2));
    TEST_ASSERT_EQUAL_FLOAT(1.f, zoomT(1000, 5000));
    TEST_ASSERT_EQUAL_FLOAT(0.f, flashT(100, 300, 90));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, flashT(100, 145, 90));
}


// ---- v1.2 Task 7: file pages, presets, settings ----
void test_every_preset_loads_clean() {
    int n = 0;
    const Preset* ps = presets(&n);
    TEST_ASSERT_TRUE(n >= 5);
    for (int i = 0; i < n; i++) {
        Patch q;
        const LoadReport r = patchFromJson(ps[i].json, q, nullptr);
        TEST_ASSERT_EQUAL_MESSAGE((int)LoadError::None, (int)r.error, ps[i].name);
        TEST_ASSERT_EQUAL_MESSAGE(0, r.skippedModules, ps[i].name);
        TEST_ASSERT_EQUAL_MESSAGE(0, r.skippedCables, ps[i].name);
        TEST_ASSERT_TRUE_MESSAGE(q.moduleCount() >= 3, ps[i].name);
        TEST_ASSERT_TRUE_MESSAGE(strlen(ps[i].name) <= 6, ps[i].name);
    }
    Patch b;
    patchFromJson(ps[0].json, b, nullptr);
    TEST_ASSERT_EQUAL_STRING("BASIC", ps[0].name);
    TEST_ASSERT_EQUAL_FLOAT(2.f, b.findAt(0, 13)->params[0]);  // OSC wave SAW: integers load as params
}

void test_preset_page_loads_undoable() {
    Fx f;
    f.side(Side::Esc);
    f.side(Side::Tab);
    f.side(Side::Tab);
    TEST_ASSERT_EQUAL(2, f.ui.state().filePage);
    f.tap(0, 0);
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    TEST_ASSERT_TRUE(gPatch.moduleCount() >= 5);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
}

void test_new_patch_undoable() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.side(Side::Esc);
    f.tap(11, 3);  // NEW
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(0, gPatch.moduleCount());
    TEST_ASSERT_EQUAL(View::Patch, f.ui.state().view);
    f.side(Side::Backslash);
    TEST_ASSERT_EQUAL(1, gPatch.moduleCount());
}

void test_first_boot_loads_basic() {
    Fx f;
    TEST_ASSERT_TRUE(f.ui.bootLoad());
    TEST_ASSERT_TRUE(gPatch.moduleCount() >= 5);
    TEST_ASSERT_FALSE(f.ui.state().dirty);
    TEST_ASSERT_EQUAL(0, gUndo.size());
}

void test_file_tab_pages() {
    Fx f;
    f.side(Side::Esc);
    for (int i = 1; i <= 4; i++) {
        f.side(Side::Tab);
        TEST_ASSERT_EQUAL(i % 4, f.ui.state().filePage);
    }
    f.sideDown(Side::Fn);
    f.side(Side::Tab);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(3, f.ui.state().filePage);
}

void test_slot_36_save() {
    Fx f;
    f.place(TOsc, 1, 1);
    toFileSave(f);
    f.slot(11);  // row 0, col 11 = slot 12
    f.tap(11, 2);  // row 2, col 11 = slot 36
    f.side(Side::Enter);
    TEST_ASSERT_EQUAL(1, (int)gStore.files.count("/modsynth/patches/slot36.json"));
}

void test_system_settings_saved() {
    Fx f;
    f.side(Side::Esc);
    for (int i = 0; i < 3; i++) f.side(Side::Tab);
    TEST_ASSERT_EQUAL(3, f.ui.state().filePage);
    const uint32_t gen = f.ui.state().settingsGen;
    f.tap(0, 0);  // theme toggles on a tap
    TEST_ASSERT_EQUAL(1, f.ui.state().settings.theme);
    f.tap(3, 0);  // brightness 2
    TEST_ASSERT_EQUAL(2, f.ui.state().settings.brightness);
    f.tap(10, 0);  // hold 600 ms
    TEST_ASSERT_EQUAL(600, f.ui.holdMs());
    TEST_ASSERT_TRUE(f.ui.state().settingsGen != gen);
    TEST_ASSERT_EQUAL(1, (int)gStore.files.count("/modsynth/settings.json"));
    Fx g;  // a fresh controller reads them back
    gStore.files["/modsynth/settings.json"] = "{\"theme\":1,\"bright\":2,\"hold\":600}";
    g.ui.loadSettings();
    TEST_ASSERT_EQUAL(1, g.ui.state().settings.theme);
    TEST_ASSERT_EQUAL(600, g.ui.holdMs());
}


void test_bit_cell_tap_toggles() {
    Fx f;
    f.sideDown(Side::Fn);
    f.tap(0, 1);
    f.sideUp(Side::Fn);
    f.tileTap(registryIndex(kTrgType));
    f.side(Side::Enter);
    f.tap(0, 1);
    f.side(Side::Enter);  // open TRG: page PATT
    const ModuleRec* m = gPatch.findAt(0, 12);
    TEST_ASSERT_TRUE(m->type == &kTrgType);
    const uint32_t before = (uint32_t)m->params[1];
    f.tap(3, 2);  // lane 2, step 4
    TEST_ASSERT_EQUAL_UINT32(before ^ (1u << 3), (uint32_t)gPatch.findAt(0, 12)->params[1]);
    f.tap(3, 2);
    TEST_ASSERT_EQUAL_UINT32(before, (uint32_t)gPatch.findAt(0, 12)->params[1]);
    f.side(Side::Backslash);  // each tap is its own undo step
    TEST_ASSERT_EQUAL_UINT32(before ^ (1u << 3), (uint32_t)gPatch.findAt(0, 12)->params[1]);
}


// ---- v1.2 Task 12: KEYS layouts ----
void test_chr_layout() {
    TEST_ASSERT_EQUAL(36, keysNoteAt(kLayChr, 0, 0, 3, 0));
    TEST_ASSERT_EQUAL(47, keysNoteAt(kLayChr, 0, 0, 3, 11));
    TEST_ASSERT_EQUAL(72, keysNoteAt(kLayChr, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL(-1, keysNoteAt(kLayChr, 0, 0, 4, 0));
}

void test_bass_fret_layout() {
    TEST_ASSERT_EQUAL(28, keysNoteAt(kLayBas, 0, 0, 3, 0));  // E1
    TEST_ASSERT_EQUAL(33, keysNoteAt(kLayBas, 0, 0, 2, 0));  // A1
    TEST_ASSERT_EQUAL(48, keysNoteAt(kLayBas, 0, 0, 0, 5));  // G2 string, 5th fret = C3
    TEST_ASSERT_EQUAL(keysNoteAt(kLayBas, 0, 0, 3, 5), keysNoteAt(kLayBas, 0, 0, 2, 0));  // 5th fret = next string
}

void test_scale_layout_in_key() {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 12; c++) {
            const int n = keysNoteAt(kLayScl, 1, 2, r, c);  // D major
            const int pc = ((n - 2) % 12 + 12) % 12;
            TEST_ASSERT_TRUE((scaleMask(1) >> pc) & 1u);
        }
    TEST_ASSERT_EQUAL(38, keysNoteAt(kLayScl, 1, 2, 3, 0));  // root D2
    TEST_ASSERT_EQUAL(50, keysNoteAt(kLayScl, 1, 2, 2, 0));  // a row up = an octave up
}

void test_pno_black_gaps() {
    TEST_ASSERT_EQUAL(48, keysNoteAt(kLayPno, 0, 0, 3, 0));  // C3
    TEST_ASSERT_EQUAL(50, keysNoteAt(kLayPno, 0, 0, 3, 1));  // D3
    TEST_ASSERT_EQUAL(49, keysNoteAt(kLayPno, 0, 0, 2, 0));  // C#3 above the C/D gap
    TEST_ASSERT_EQUAL(-1, keysNoteAt(kLayPno, 0, 0, 2, 2));  // none after E
    TEST_ASSERT_EQUAL(60, keysNoteAt(kLayPno, 0, 0, 3, 7));  // the next C
    TEST_ASSERT_EQUAL(60, keysNoteAt(kLayPno, 0, 0, 1, 0));  // upper piano from C4
    TEST_ASSERT_EQUAL(61, keysNoteAt(kLayPno, 0, 0, 0, 0));
}

void test_pad_and_4th_layouts() {
    TEST_ASSERT_EQUAL(36, keysNoteAt(kLayPad, 0, 0, 3, 0));
    TEST_ASSERT_EQUAL(39, keysNoteAt(kLayPad, 0, 0, 3, 3));
    TEST_ASSERT_EQUAL(40, keysNoteAt(kLayPad, 0, 0, 2, 0));
    TEST_ASSERT_EQUAL(52, keysNoteAt(kLayPad, 0, 0, 3, 4));
    TEST_ASSERT_EQUAL(41, keysNoteAt(kLay4th, 0, 0, 2, 0));
}

void test_layout_switch_releases_notes() {
    Fx f;
    openKeysPlay(f);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.down(3, 2);
    f.side(Side::Enter);  // next layout
    TEST_ASSERT_EQUAL_FLOAT(1.f, gPatch.findAt(0, 0)->params[4]);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
    f.up(3, 2);
    f.sideDown(Side::Fn);
    f.side(Side::Enter);  // previous
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL_FLOAT(0.f, gPatch.findAt(0, 0)->params[4]);
    f.sideDown(Side::Fn);
    f.side(Side::Enter);  // wraps to the last layout
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL_FLOAT((float)(kLayCount - 1), gPatch.findAt(0, 0)->params[4]);
}

void test_ctrl_space_octave() {
    Fx f;
    openKeysPlay(f);
    f.side(Side::Space);
    TEST_ASSERT_EQUAL_FLOAT(1.f, gPatch.findAt(0, 0)->params[0]);
    f.side(Side::Ctrl);
    f.side(Side::Ctrl);
    TEST_ASSERT_EQUAL_FLOAT(-1.f, gPatch.findAt(0, 0)->params[0]);
}

void test_latch_holds() {
    Fx f;
    openKeysPlay(f);
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.side(Side::Backslash);  // latch on
    TEST_ASSERT_EQUAL_FLOAT(1.f, gPatch.findAt(0, 0)->params[7]);
    f.tap(0, 3);
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));  // still sounding after the release
    TEST_ASSERT_EQUAL_FLOAT(36.f, k->monitor(0));
    f.tap(2, 3);  // another key moves the held note
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(38.f, k->monitor(0));
    f.tap(2, 3);  // the same key again releases it
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(0.f, k->monitor(1));
}


// ---- v1.2 review round 1 ----
void test_rollover_is_not_a_chord() {
    Fx f;
    f.place(TOsc, 1, 1);
    f.down(1, 1);
    f.down(5, 3);  // 10 ms later: two quick taps
    f.up(1, 1);
    f.up(5, 3);
    TEST_ASSERT_NOT_NULL(gPatch.findAt(0, 13));
    TEST_ASSERT_NULL(gPatch.findAt(0, 41));
}

void test_chord_pins_refused_leaves_no_carry() {
    Fx f;
    openModule(f, TVcf, 0, 0);
    for (int k = 0; k < 2; k++) {  // the second time it is ALREADY PATCHED
        f.down(0, 3);
        f.t += 150;
        f.down(1, 2);
        f.up(1, 2);
        f.up(0, 3);
    }
    TEST_ASSERT_EQUAL(1, gPatch.cableCount());
    TEST_ASSERT_EQUAL_STRING("ALREADY PATCHED", f.ui.state().toast);
    TEST_ASSERT_FALSE(f.ui.state().carry.active);
}

void test_play_page_ignores_unseen_release() {
    Fx f;
    f.place(TKeys, 0, 0);
    f.hold(0, 0);  // the opening key is still down when PLAY opens
    TEST_ASSERT_TRUE(f.ui.onPlayPage());
    Module* k = f.eng.instance(gPatch.findAt(0, 0)->id);
    float buf[kBlock];
    f.down(5, 3);
    f.eng.processBlock(buf);
    f.up(0, 0);  // releasing the opening key must not end the note
    f.eng.processBlock(buf);
    f.eng.processBlock(buf);
    TEST_ASSERT_EQUAL_FLOAT(1.f, k->monitor(1));
    f.up(5, 3);
}

void test_copy_at_module_limit_no_undo_step() {
    Fx f;
    for (int i = 0; i < kMaxModules; i++) gPatch.addModule(kVcaType, (uint8_t)(i / 12), (uint8_t)(i % 12), nullptr);
    f.eng.apply(gPatch);
    f.tap(0, 0);
    f.side(Side::Ctrl);
    const int undoBefore = gUndo.size();
    f.sideDown(Side::Fn);
    f.tap(5, 3);
    f.sideUp(Side::Fn);
    TEST_ASSERT_EQUAL(undoBefore, gUndo.size());
    TEST_ASSERT_EQUAL_STRING("TOO MANY", f.ui.state().toast);
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
    RUN_TEST(test_hold_enter_on_pin_carries_not_sweeps);
    RUN_TEST(test_tilt_deadband);
    RUN_TEST(test_no_autosave_mid_tilt);
    RUN_TEST(test_hold_module_opens_it);
    RUN_TEST(test_fn_tap_module_opens_it);
    RUN_TEST(test_hold_enter_in_patch_view_opens_once);
    RUN_TEST(test_place_by_hold_tile);
    RUN_TEST(test_place_by_fn_tile);
    RUN_TEST(test_hold_pin_picks_up_cable);
    RUN_TEST(test_enter_on_pin_picks_up_cable);
    RUN_TEST(test_hold_stepped_param_cycles);
    RUN_TEST(test_hold_plain_param_does_nothing);
    RUN_TEST(test_file_tap_selects_then_enter_saves);
    RUN_TEST(test_file_hold_slot_loads);
    RUN_TEST(test_file_slot_cleared_on_open);
    RUN_TEST(test_keys_settings_enter_is_do_it);
    RUN_TEST(test_place_pages_by_category);
    RUN_TEST(test_place_tab_flips_category);
    RUN_TEST(test_all_blurbs_short);
    RUN_TEST(test_place_cell_of_inverts);
    RUN_TEST(test_carry_out_of_keys_settings);
    RUN_TEST(test_hold_enter_on_stepped_param_steps);
    RUN_TEST(test_file_tab_clears_slot);
    RUN_TEST(test_ctrl_hold_pin_still_carries_in_module_view);
    RUN_TEST(test_select_during_sweep_ends_it);
    RUN_TEST(test_coarse_tap_during_sweep_ends_it);
    RUN_TEST(test_strip_fill_partial_segment);
    RUN_TEST(test_strip_fill_ends);
    RUN_TEST(test_input_amount_edit);
    RUN_TEST(test_auto_pages_overflow);
    RUN_TEST(test_drawing_page0_width5);
    RUN_TEST(test_keys_pages);
    RUN_TEST(test_fn_tab_previous_page);
    RUN_TEST(test_tab_flip_releases_notes);
    RUN_TEST(test_chord_moves_without_opening);
    RUN_TEST(test_hold_then_press_opens_only);
    RUN_TEST(test_chord_swaps);
    RUN_TEST(test_ctrl_after_select_moves);
    RUN_TEST(test_move_onto_module_swaps);
    RUN_TEST(test_fn_target_copies);
    RUN_TEST(test_fn_tap_releases_carry);
    RUN_TEST(test_fn_pin_still_drops);
    RUN_TEST(test_chord_pins_patch);
    RUN_TEST(test_redo_fn_backslash);
    RUN_TEST(test_side_held_tracked);
    RUN_TEST(test_icons_move_esc_cancel);
    RUN_TEST(test_icons_carry_fn_active);
    RUN_TEST(test_icons_enter_follows_selection);
    RUN_TEST(test_icons_tab_page_text);
    RUN_TEST(test_icons_fn_held_redo);
    RUN_TEST(test_help_visible_after_fn_hold);
    RUN_TEST(test_side_pressed_flash);
    RUN_TEST(test_ease_converges);
    RUN_TEST(test_zoom_t_bounds);
    RUN_TEST(test_every_preset_loads_clean);
    RUN_TEST(test_preset_page_loads_undoable);
    RUN_TEST(test_new_patch_undoable);
    RUN_TEST(test_first_boot_loads_basic);
    RUN_TEST(test_file_tab_pages);
    RUN_TEST(test_slot_36_save);
    RUN_TEST(test_system_settings_saved);
    RUN_TEST(test_bit_cell_tap_toggles);
    RUN_TEST(test_chr_layout);
    RUN_TEST(test_bass_fret_layout);
    RUN_TEST(test_scale_layout_in_key);
    RUN_TEST(test_pno_black_gaps);
    RUN_TEST(test_pad_and_4th_layouts);
    RUN_TEST(test_layout_switch_releases_notes);
    RUN_TEST(test_ctrl_space_octave);
    RUN_TEST(test_latch_holds);
    RUN_TEST(test_rollover_is_not_a_chord);
    RUN_TEST(test_chord_pins_refused_leaves_no_carry);
    RUN_TEST(test_play_page_ignores_unseen_release);
    RUN_TEST(test_copy_at_module_limit_no_undo_step);
    return UNITY_END();
}
