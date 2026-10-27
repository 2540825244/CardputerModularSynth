#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "core/modules/dly.h"
#include "core/modules/env.h"
#include "core/modules/keys.h"
#include "core/modules/lfo.h"
#include "core/modules/osc.h"
#include "core/modules/out.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"
#include "core/patch.h"

void setUp() {}
void tearDown() {}

void test_add_find() {
    Patch p;
    uint16_t id = 0;
    TEST_ASSERT_EQUAL(PatchError::None, p.addModule(kOscType, 0, 14, &id));
    const ModuleRec* m = p.findAt(0, 14);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_PTR(&kOscType, m->type);
    TEST_ASSERT_EQUAL_UINT16(id, m->id);
    for (int i = 0; i < kOscType.numParams; i++) TEST_ASSERT_EQUAL_FLOAT(kOscType.params[i].def, m->params[i]);
    TEST_ASSERT_EQUAL_FLOAT(1.f, m->outAmount[0]);
    TEST_ASSERT_EQUAL(PatchError::CellTaken, p.addModule(kVcaType, 0, 14, nullptr));
    TEST_ASSERT_EQUAL(1, p.moduleCount());
    TEST_ASSERT_NULL(p.findAt(1, 14));
}

void test_limits() {
    Patch p;
    for (int i = 0; i < kMaxModules; i++)
        TEST_ASSERT_EQUAL(PatchError::None, p.addModule(kVcaType, i / kCellsPerPage, i % kCellsPerPage, nullptr));
    TEST_ASSERT_EQUAL(PatchError::TooManyModules, p.addModule(kVcaType, 2, 40, nullptr));

    Patch q;
    TEST_ASSERT_EQUAL(PatchError::None, q.addModule(kOutType, 0, 0, nullptr));
    TEST_ASSERT_EQUAL(PatchError::MaxInstances, q.addModule(kOutType, 0, 1, nullptr));

    // 8 sources x 8 destinations = 64 distinct cables
    Patch c;
    uint16_t src[8], dst[8];
    for (int i = 0; i < 8; i++) {
        c.addModule(kVcaType, 0, i, &src[i]);
        c.addModule(kVcaType, 1, i, &dst[i]);
    }
    for (int a = 0; a < 8; a++)
        for (int b = 0; b < 8; b++)
            TEST_ASSERT_EQUAL(PatchError::None, c.addCable(src[a], 0, dst[b], 0));
    TEST_ASSERT_EQUAL(64, c.cableCount());
    TEST_ASSERT_EQUAL(PatchError::TooManyCables, c.addCable(src[0], 0, dst[0], 1));
}

void test_cable_rules() {
    Patch p;
    uint16_t o, v;
    p.addModule(kOscType, 0, 0, &o);
    p.addModule(kVcaType, 0, 1, &v);
    TEST_ASSERT_EQUAL(PatchError::None, p.addCable(o, 0, v, 0));
    TEST_ASSERT_EQUAL(PatchError::Duplicate, p.addCable(o, 0, v, 0));
    TEST_ASSERT_EQUAL(PatchError::BadPin, p.addCable(o, kOscType.numOuts, v, 0));
    TEST_ASSERT_EQUAL(PatchError::BadPin, p.addCable(o, 0, v, kVcaType.numIns));
    TEST_ASSERT_EQUAL(PatchError::BadPin, p.addCable(999, 0, v, 0));
    TEST_ASSERT_EQUAL(PatchError::None, p.addCable(o, 0, o, 1));  // self-cable OSC.out -> OSC.fm
    TEST_ASSERT_EQUAL(2, p.cableCount());
}

void test_cable_colours() {
    Patch p;
    uint16_t o, v;
    p.addModule(kOscType, 0, 0, &o);
    p.addModule(kVcaType, 0, 1, &v);
    uint16_t w;
    p.addModule(kVcaType, 0, 2, &w);
    // 7 distinct cables: o.out -> v.in0, v.in1, w.in0, w.in1, and v.out -> v.in0.. etc.
    const struct { uint16_t f; uint8_t fo; uint16_t t; uint8_t ti; } cs[] = {
        {o, 0, v, 0}, {o, 0, v, 1}, {o, 0, w, 0}, {o, 0, w, 1}, {v, 0, v, 0}, {v, 0, v, 1}, {w, 0, v, 0}};
    for (const auto& c : cs) TEST_ASSERT_EQUAL(PatchError::None, p.addCable(c.f, c.fo, c.t, c.ti));
    const uint8_t want[] = {0, 1, 2, 3, 4, 5, 0};
    for (int i = 0; i < 7; i++) TEST_ASSERT_EQUAL_UINT8(want[i], p.cableAt(i).color);
    // removing the colour-2 cable (o -> w.in0) frees colour 2 for the next one
    TEST_ASSERT_EQUAL(1, p.removeCablesAtPin(w, false, 0));  // only o -> w.in0 ends there
    TEST_ASSERT_EQUAL(6, p.cableCount());
    TEST_ASSERT_EQUAL(PatchError::None, p.addCable(w, 0, w, 0));
    TEST_ASSERT_EQUAL_UINT8(2, p.cableAt(6).color);
}

void test_remove_module_removes_cables() {
    Patch p;
    uint16_t o, f, a;
    p.addModule(kOscType, 0, 0, &o);
    p.addModule(kVcfType, 0, 1, &f);
    p.addModule(kVcaType, 0, 2, &a);
    TEST_ASSERT_EQUAL(PatchError::None, p.addCable(o, 0, f, 0));
    TEST_ASSERT_EQUAL(PatchError::None, p.addCable(f, 0, a, 0));
    TEST_ASSERT_TRUE(p.removeModule(f));
    TEST_ASSERT_EQUAL(0, p.cableCount());
    TEST_ASSERT_NOT_NULL(p.find(o));
    TEST_ASSERT_NOT_NULL(p.find(a));
    TEST_ASSERT_NULL(p.find(f));
    TEST_ASSERT_FALSE(p.removeModule(f));
    uint16_t id2;
    p.addModule(kVcfType, 0, 1, &id2);
    TEST_ASSERT_TRUE(id2 != f);  // ids never reused
}

void test_move() {
    Patch p;
    uint16_t a, b;
    p.addModule(kOscType, 0, 5, &a);
    p.addModule(kVcaType, 0, 6, &b);
    TEST_ASSERT_EQUAL(PatchError::None, p.moveModule(a, 2, 7));
    TEST_ASSERT_EQUAL_UINT16(a, p.findAt(2, 7)->id);
    TEST_ASSERT_NULL(p.findAt(0, 5));
    TEST_ASSERT_EQUAL(PatchError::CellTaken, p.moveModule(a, 0, 6));
    TEST_ASSERT_EQUAL_UINT8(2, p.find(a)->page);
    TEST_ASSERT_EQUAL_UINT8(7, p.find(a)->cell);
}

void test_memcmp_deterministic() {
    Patch a, b;
    TEST_ASSERT_EQUAL(0, memcmp(&a, &b, sizeof(Patch)));
    uint16_t o, v;
    b.addModule(kOscType, 0, 0, &o);
    b.addModule(kVcaType, 0, 1, &v);
    b.addCable(o, 0, v, 0);
    b.removeModule(o);
    b.removeModule(v);
    b.clear();
    a.clear();
    TEST_ASSERT_EQUAL(0, memcmp(&a, &b, sizeof(Patch)));
    // same edits in two patches give identical bytes, also after removals
    Patch c, d;
    for (Patch* p : {&c, &d}) {
        p->addModule(kOscType, 0, 0, &o);
        p->addModule(kVcaType, 0, 1, &v);
        p->addCable(o, 0, v, 0);
        p->addCable(o, 0, v, 1);
        p->removeCablesAtPin(v, false, 1);
    }
    TEST_ASSERT_EQUAL(0, memcmp(&c, &d, sizeof(Patch)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_add_find);
    RUN_TEST(test_limits);
    RUN_TEST(test_cable_rules);
    RUN_TEST(test_cable_colours);
    RUN_TEST(test_remove_module_removes_cables);
    RUN_TEST(test_move);
    RUN_TEST(test_memcmp_deterministic);
    return UNITY_END();
}
