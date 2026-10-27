#include <unity.h>
#include <cmath>
#include <cstring>
#include <map>
#include "core/modules/dly.h"
#include "core/modules/env.h"
#include "core/modules/keys.h"
#include "core/modules/osc.h"
#include "core/modules/out.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"
#include "core/patch_io.h"
#include "core/registry.h"
#include "core/storage.h"

void setUp() {}
void tearDown() {}

struct MemStorage : Storage {
    std::map<std::string, std::string> files;
    bool ok = true;
    bool available() override { return ok; }
    bool read(const char* p, std::string& out) override {
        auto it = files.find(p);
        if (!ok || it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool write(const char* p, const std::string& d) override {
        if (!ok) return false;
        files[p] = d;
        return true;
    }
    bool exists(const char* p) override { return ok && files.count(p) > 0; }
};

static uint32_t bigHeap() { return 1000000; }

static uint16_t add(Patch& p, const ModuleTypeInfo& t, int cell) {
    uint16_t id = 0;
    TEST_ASSERT_EQUAL(PatchError::None, p.addModule(t, 0, cell, &id));
    return id;
}

static Patch voice() {
    Patch p;
    uint16_t k = add(p, kKeysType, 0), o = add(p, kOscType, 14), f = add(p, kVcfType, 2),
             a = add(p, kVcaType, 3), e = add(p, kEnvType, 4), out = add(p, kOutType, 5);
    p.addCable(k, 0, o, 0);
    p.addCable(o, 0, f, 0);
    p.addCable(f, 0, a, 0);
    p.addCable(k, 1, e, 0);
    p.addCable(e, 0, a, 1);
    p.addCable(a, 0, out, 0);
    p.find(o)->params[0] = 3.f;
    p.find(f)->params[0] = 733.123456f;
    p.find(f)->params[1] = 0.37654321f;
    p.find(e)->params[0] = 12.3456f;
    p.find(a)->outAmount[0] = -0.4321f;
    return p;
}

void test_slot_path() {
    TEST_ASSERT_EQUAL_STRING("/modsynth/patches/slot01.json", slotPath(1).c_str());
    TEST_ASSERT_EQUAL_STRING("/modsynth/patches/slot12.json", slotPath(12).c_str());
    TEST_ASSERT_EQUAL_STRING("/modsynth/patches/autosave.json", slotPath(0).c_str());
    TEST_ASSERT_EQUAL_STRING("", slotPath(13).c_str());
}

void test_roundtrip() {
    Patch p = voice();
    MemStorage st;
    st.write(slotPath(1).c_str(), patchToJson(p));
    std::string json;
    TEST_ASSERT_TRUE(st.read(slotPath(1).c_str(), json));
    Patch q;
    LoadReport r = patchFromJson(json, q, nullptr);
    TEST_ASSERT_EQUAL(LoadError::None, r.error);
    TEST_ASSERT_EQUAL(0, r.skippedModules + r.skippedCables);
    TEST_ASSERT_EQUAL(p.moduleCount(), q.moduleCount());
    for (int i = 0; i < p.moduleCount(); i++) {
        const ModuleRec &a = p.moduleAt(i), &b = q.moduleAt(i);
        TEST_ASSERT_EQUAL_UINT16(a.id, b.id);
        TEST_ASSERT_EQUAL_PTR(a.type, b.type);
        TEST_ASSERT_EQUAL_UINT8(a.page, b.page);
        TEST_ASSERT_EQUAL_UINT8(a.cell, b.cell);
        for (int k = 0; k < a.type->numParams; k++)
            TEST_ASSERT_FLOAT_WITHIN(1e-6f * fmaxf(1.f, fabsf(a.params[k])), a.params[k], b.params[k]);
        for (int o = 0; o < a.type->numOuts; o++) TEST_ASSERT_FLOAT_WITHIN(1e-6f, a.outAmount[o], b.outAmount[o]);
    }
    TEST_ASSERT_EQUAL(p.cableCount(), q.cableCount());
    for (int i = 0; i < p.cableCount(); i++) {
        TEST_ASSERT_EQUAL_UINT16(p.cableAt(i).fromId, q.cableAt(i).fromId);
        TEST_ASSERT_EQUAL_UINT8(p.cableAt(i).fromOut, q.cableAt(i).fromOut);
        TEST_ASSERT_EQUAL_UINT16(p.cableAt(i).toId, q.cableAt(i).toId);
        TEST_ASSERT_EQUAL_UINT8(p.cableAt(i).toIn, q.cableAt(i).toIn);
        TEST_ASSERT_EQUAL_UINT8(p.cableAt(i).color, q.cableAt(i).color);
    }
    TEST_ASSERT_EQUAL_UINT16(p.nextId, q.nextId);
}

void test_bad_json() {
    std::string json = patchToJson(voice());
    Patch target = voice();
    Patch before = target;
    LoadReport r = patchFromJson(json.substr(0, json.size() / 2), target, nullptr);
    TEST_ASSERT_EQUAL(LoadError::BadJson, r.error);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &target, sizeof(Patch)));
    TEST_ASSERT_EQUAL(LoadError::BadJson, patchFromJson("[1,2]", target, nullptr).error);
    TEST_ASSERT_EQUAL(LoadError::BadJson, patchFromJson("", target, nullptr).error);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &target, sizeof(Patch)));
}

void test_sanitize_values() {
    const std::string json =
        "{\"format\":1,\"modules\":["
        "{\"id\":1,\"type\":\"vcf\",\"page\":0,\"cell\":0,\"params\":{\"cutoff\":1e9,\"res\":\"x\"}},"
        "{\"id\":2,\"type\":\"osc\",\"page\":0,\"cell\":1,\"params\":{\"wave\":7},\"amounts\":{\"out\":-5,\"bogus\":3}}"
        "],\"cables\":[]}";
    Patch p;
    TEST_ASSERT_EQUAL(LoadError::None, patchFromJson(json, p, nullptr).error);
    TEST_ASSERT_EQUAL_FLOAT(20000.f, p.find(1)->params[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, p.find(1)->params[1]);  // junk -> default
    TEST_ASSERT_EQUAL_FLOAT(3.f, p.find(2)->params[0]);
    TEST_ASSERT_EQUAL_FLOAT(-1.f, p.find(2)->outAmount[0]);
}

void test_unknown_and_dangling() {
    const std::string json =
        "{\"format\":1,\"modules\":["
        "{\"id\":1,\"type\":\"osc\",\"page\":0,\"cell\":0},"
        "{\"id\":2,\"type\":\"plaits\",\"page\":0,\"cell\":1},"
        "{\"id\":3,\"type\":\"vca\",\"page\":0,\"cell\":2}],"
        "\"cables\":[{\"from\":[2,\"out\"],\"to\":[3,\"in\"],\"color\":1},"
        "{\"from\":[1,\"out\"],\"to\":[3,\"nope\"],\"color\":1},"
        "{\"from\":[1,\"out\"],\"to\":[3,\"in\"],\"color\":4}]}";
    Patch p;
    LoadReport r = patchFromJson(json, p, nullptr);
    TEST_ASSERT_EQUAL(LoadError::None, r.error);
    TEST_ASSERT_EQUAL(1, r.skippedModules);
    TEST_ASSERT_EQUAL(2, r.skippedCables);
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_EQUAL(1, p.cableCount());
    TEST_ASSERT_EQUAL_UINT8(4, p.cableAt(0).color);
    TEST_ASSERT_EQUAL_UINT16(4, p.nextId);
}

void test_cell_collision_and_instances() {
    const std::string json =
        "{\"format\":1,\"modules\":["
        "{\"id\":1,\"type\":\"osc\",\"page\":0,\"cell\":0},"
        "{\"id\":2,\"type\":\"vca\",\"page\":0,\"cell\":0},"
        "{\"id\":3,\"type\":\"out\",\"page\":0,\"cell\":1},"
        "{\"id\":4,\"type\":\"out\",\"page\":0,\"cell\":2}],\"cables\":[]}";
    Patch p;
    LoadReport r = patchFromJson(json, p, nullptr);
    TEST_ASSERT_EQUAL(2, r.skippedModules);
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_EQUAL_PTR(&kOscType, p.findAt(0, 0)->type);
    TEST_ASSERT_NOT_NULL(p.find(3));
    TEST_ASSERT_NULL(p.find(4));
}

void test_newer_format() {
    Patch p = voice();
    Patch before = p;
    LoadReport r = patchFromJson("{\"format\":2,\"modules\":[]}", p, nullptr);
    TEST_ASSERT_EQUAL(LoadError::NewerFormat, r.error);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &p, sizeof(Patch)));
}

void test_empty() {
    Patch p = voice();
    TEST_ASSERT_EQUAL(LoadError::None, patchFromJson("{\"format\":1,\"modules\":[]}", p, nullptr).error);
    TEST_ASSERT_EQUAL(0, p.moduleCount());
    Patch q = voice();
    Patch before = q;
    LoadReport r = patchFromJson(
        "{\"format\":1,\"modules\":[{\"id\":1,\"type\":\"zzz\",\"page\":0,\"cell\":0}]}", q, nullptr);
    TEST_ASSERT_EQUAL(LoadError::Empty, r.error);
    TEST_ASSERT_EQUAL(1, r.skippedModules);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &q, sizeof(Patch)));
}

void test_size_bounds() {
    // maximal patch: 32 modules (a mix with many params), 64 cables
    Patch p;
    uint16_t src[8], dst[8];
    for (int i = 0; i < 8; i++) {
        p.addModule(kVcfType, i / 4, i % 4, &src[i]);
        p.addModule(kEnvType, 2, i, &dst[i]);
    }
    for (int i = 0; i < 16; i++) p.addModule(kOscType, 2, 8 + i, nullptr);
    TEST_ASSERT_EQUAL(32, p.moduleCount());
    for (int a = 0; a < 8; a++)
        for (int b = 0; b < 8; b++) TEST_ASSERT_EQUAL(PatchError::None, p.addCable(src[a], 0, dst[b], 0));
    for (int i = 0; i < p.moduleCount(); i++) {
        ModuleRec* r = p.find(p.moduleAt(i).id);
        for (int k = 0; k < r->type->numParams; k++) r->params[k] = r->type->params[k].min + 0.123456789f;
        r->outAmount[0] = -0.123456789f;
    }
    const std::string json = patchToJson(p);
    TEST_ASSERT_TRUE(json.size() <= 16384);
    Patch q;
    TEST_ASSERT_EQUAL(LoadError::None, patchFromJson(json, q, nullptr).error);
    TEST_ASSERT_EQUAL(32, q.moduleCount());
    TEST_ASSERT_EQUAL(64, q.cableCount());

    Patch t = voice();
    Patch before = t;
    std::string big = "{\"format\":1,\"junk\":\"" + std::string(20000, 'x') + "\",\"modules\":[]}";
    TEST_ASSERT_EQUAL(LoadError::BadJson, patchFromJson(big, t, nullptr).error);
    std::string deep = "{\"format\":1,\"modules\":" + std::string(500, '[') + std::string(500, ']') + "}";
    TEST_ASSERT_EQUAL(LoadError::BadJson, patchFromJson(deep, t, nullptr).error);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &t, sizeof(Patch)));
}

void test_id_range() {
    const std::string json =
        "{\"format\":1,\"modules\":["
        "{\"id\":65535,\"type\":\"osc\",\"page\":0,\"cell\":0},"
        "{\"id\":65280,\"type\":\"osc\",\"page\":0,\"cell\":1},"
        "{\"id\":65279,\"type\":\"osc\",\"page\":0,\"cell\":3},"
        "{\"id\":65278,\"type\":\"vca\",\"page\":0,\"cell\":2}]}";
    Patch p;
    LoadReport r = patchFromJson(json, p, nullptr);
    TEST_ASSERT_EQUAL(LoadError::None, r.error);
    TEST_ASSERT_EQUAL(2, r.skippedModules);
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_EQUAL_UINT16(65280, p.nextId);
    // the id space is exhausted: new modules are refused instead of wrapping onto old ids
    TEST_ASSERT_EQUAL(PatchError::BadPin, p.addModule(kVcaType, 0, 5, nullptr));
    TEST_ASSERT_EQUAL_UINT16(65280, p.nextId);
}

void test_budget_skip() {
    const std::string json =
        "{\"format\":1,\"modules\":["
        "{\"id\":1,\"type\":\"dly\",\"page\":0,\"cell\":0},"
        "{\"id\":2,\"type\":\"dly\",\"page\":0,\"cell\":1},"
        "{\"id\":3,\"type\":\"dly\",\"page\":0,\"cell\":2}],\"cables\":[]}";
    Engine eng(bigHeap);
    Patch p;
    LoadReport r = patchFromJson(json, p, &eng);
    TEST_ASSERT_EQUAL(LoadError::None, r.error);
    TEST_ASSERT_EQUAL(2, p.moduleCount());
    TEST_ASSERT_EQUAL(1, r.skippedModules);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_slot_path);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_bad_json);
    RUN_TEST(test_sanitize_values);
    RUN_TEST(test_unknown_and_dangling);
    RUN_TEST(test_cell_collision_and_instances);
    RUN_TEST(test_newer_format);
    RUN_TEST(test_empty);
    RUN_TEST(test_size_bounds);
    RUN_TEST(test_id_range);
    RUN_TEST(test_budget_skip);
    return UNITY_END();
}
