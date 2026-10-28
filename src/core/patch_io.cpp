#include "patch_io.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include "registry.h"

namespace {

constexpr int kFormat = 1;
constexpr size_t kMaxPatchJson = 16384;  // a full 32-module, 64-cable patch is about 12 KB
constexpr int kMaxStoredId = 0xFEFF;     // keeps nextId from wrapping

int findKey(const char* const* keys, int n, const char* k) {
    if (!keys) return -1;
    for (int i = 0; i < n; i++)
        if (strcmp(keys[i], k) == 0) return i;
    return -1;
}

// Resolves a cable end [id, "pin"] to (record, pin index); false if anything is missing.
bool resolveEnd(const Patch& p, JsonVariantConst v, bool output, uint16_t* id, uint8_t* pin) {
    JsonArrayConst a = v.as<JsonArrayConst>();
    if (a.isNull() || a.size() != 2 || !a[0].is<int>() || !a[1].is<const char*>()) return false;
    const int rid = a[0].as<int>();
    if (rid < 0 || rid > 0xFFFF) return false;
    const ModuleRec* r = p.find(static_cast<uint16_t>(rid));
    if (!r) return false;
    const ModuleTypeInfo& t = *r->type;
    const int idx = output ? findKey(t.outKeys, t.numOuts, a[1].as<const char*>())
                           : findKey(t.inKeys, t.numIns, a[1].as<const char*>());
    if (idx < 0) return false;
    *id = static_cast<uint16_t>(rid);
    *pin = static_cast<uint8_t>(idx);
    return true;
}

}  // namespace

std::string slotPath(int slot) {
    char buf[64];
    if (slot == 0) {
        snprintf(buf, sizeof buf, "%s/autosave.json", kPatchDir);
    } else if (slot >= 1 && slot <= kSlots) {
        snprintf(buf, sizeof buf, "%s/slot%02d.json", kPatchDir, slot);
    } else {
        return "";
    }
    return buf;
}

std::string patchToJson(const Patch& p) {
    JsonDocument doc;
    doc["format"] = kFormat;
    JsonArray mods = doc["modules"].to<JsonArray>();
    for (int i = 0; i < p.moduleCount(); i++) {
        const ModuleRec& r = p.moduleAt(i);
        const ModuleTypeInfo& t = *r.type;
        JsonObject m = mods.add<JsonObject>();
        m["id"] = r.id;
        m["type"] = t.id;
        m["page"] = r.page;
        m["cell"] = r.cell;
        JsonObject ps = m["params"].to<JsonObject>();
        for (int k = 0; k < t.numParams && k < kMaxParams; k++) ps[t.params[k].key] = r.params[k];
        JsonObject am = m["amounts"].to<JsonObject>();
        for (int o = 0; o < t.numOuts && o < kMaxOuts; o++) am[t.outKeys[o]] = r.outAmount[o];
        bool anyIn = false;  // inamounts only when one differs from 1, so v1.1 files stay byte-identical
        for (int i = 0; i < t.numIns && i < kMaxIns; i++) anyIn |= r.inAmount[i] != 1.f;
        if (anyIn) {
            JsonObject ia = m["inamounts"].to<JsonObject>();
            for (int i = 0; i < t.numIns && i < kMaxIns; i++) ia[t.inKeys[i]] = r.inAmount[i];
        }
    }
    JsonArray cables = doc["cables"].to<JsonArray>();
    for (int i = 0; i < p.cableCount(); i++) {
        const CableRec& c = p.cableAt(i);
        const ModuleRec* f = p.find(c.fromId);
        const ModuleRec* t = p.find(c.toId);
        if (!f || !t) continue;
        JsonObject o = cables.add<JsonObject>();
        JsonArray from = o["from"].to<JsonArray>();
        from.add(c.fromId);
        from.add(f->type->outKeys[c.fromOut]);
        JsonArray to = o["to"].to<JsonArray>();
        to.add(c.toId);
        to.add(t->type->inKeys[c.toIn]);
        o["color"] = c.color;
    }
    std::string s;
    serializeJson(doc, s);
    return s;
}

LoadReport patchFromJson(const std::string& json, Patch& out, const Engine* budget) {
    LoadReport rep{LoadError::None, 0, 0};
    if (json.size() > kMaxPatchJson) {
        rep.error = LoadError::BadJson;
        return rep;
    }
    JsonDocument filter;  // keep only what the loader reads
    filter["format"] = true;
    JsonObject fm = filter["modules"][0].to<JsonObject>();
    for (const char* k : {"id", "type", "page", "cell", "params", "amounts", "inamounts"}) fm[k] = true;
    JsonObject fc = filter["cables"][0].to<JsonObject>();
    for (const char* k : {"from", "to", "color"}) fc[k] = true;
    JsonDocument doc;
    if (deserializeJson(doc, json, DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(6)) != DeserializationError::Ok ||
        !doc.is<JsonObject>() ||
        !doc["format"].is<int>()) {
        rep.error = LoadError::BadJson;
        return rep;
    }
    const int fmt = doc["format"].as<int>();
    if (fmt > kFormat) {
        rep.error = LoadError::NewerFormat;
        return rep;
    }
    if (fmt < 1) {
        rep.error = LoadError::BadJson;
        return rep;
    }

    std::unique_ptr<Patch> scratchPtr(new Patch());  // 4.4 KB: off the UI task stack
    Patch& scratch = *scratchPtr;
    for (JsonObjectConst m : doc["modules"].as<JsonArrayConst>()) {
        const char* type = m["type"].is<const char*>() ? m["type"].as<const char*>() : nullptr;
        const ModuleTypeInfo* t = type ? findType(type) : nullptr;
        if (!t || !m["id"].is<int>() || !m["page"].is<int>() || !m["cell"].is<int>()) {
            rep.skippedModules++;
            continue;
        }
        const int id = m["id"].as<int>(), page = m["page"].as<int>(), cell = m["cell"].as<int>();
        if (id < 0 || id > kMaxStoredId || page < 0 || page >= kPages || cell < 0 || cell >= kCellsPerPage ||
            (budget && !budget->canAdd(scratch, *t).ok) ||
            scratch.addModuleWithId(static_cast<uint16_t>(id), *t, static_cast<uint8_t>(page),
                                    static_cast<uint8_t>(cell)) != PatchError::None) {
            rep.skippedModules++;
            continue;
        }
        ModuleRec* r = scratch.find(static_cast<uint16_t>(id));
        JsonObjectConst ps = m["params"].as<JsonObjectConst>();
        for (int k = 0; k < t->numParams && k < kMaxParams; k++) {
            JsonVariantConst v = ps[t->params[k].key];
            if (v.is<float>()) r->params[k] = paramClamp(t->params[k], v.as<float>());
        }
        JsonObjectConst am = m["amounts"].as<JsonObjectConst>();
        for (int o = 0; o < t->numOuts && o < kMaxOuts; o++) {
            JsonVariantConst v = am[t->outKeys[o]];
            if (!v.is<float>()) continue;
            float a = v.as<float>();
            if (!(a == a)) continue;  // NaN keeps the default
            r->outAmount[o] = a < -1.f ? -1.f : (a > 1.f ? 1.f : a);
        }
        JsonObjectConst ia = m["inamounts"].as<JsonObjectConst>();
        for (int i = 0; i < t->numIns && i < kMaxIns; i++) {
            JsonVariantConst v = ia[t->inKeys[i]];
            if (!v.is<float>()) continue;
            const float a = v.as<float>();
            if (!(a == a)) continue;
            r->inAmount[i] = a < -1.f ? -1.f : (a > 1.f ? 1.f : a);
        }
    }
    if (scratch.moduleCount() == 0 && rep.skippedModules > 0) {  // an empty file is a valid empty patch
        rep.error = LoadError::Empty;
        return rep;
    }

    for (JsonObjectConst c : doc["cables"].as<JsonArrayConst>()) {
        uint16_t fid, tid;
        uint8_t fo, ti;
        if (!resolveEnd(scratch, c["from"], true, &fid, &fo) || !resolveEnd(scratch, c["to"], false, &tid, &ti) ||
            scratch.addCable(fid, fo, tid, ti) != PatchError::None) {
            rep.skippedCables++;
            continue;
        }
        if (c["color"].is<int>()) {
            const int col = c["color"].as<int>();
            if (col >= 0 && col < kNumCableColors)
                scratch.setCableColor(scratch.cableCount() - 1, static_cast<uint8_t>(col));
        }
    }
    out = scratch;
    return rep;
}
