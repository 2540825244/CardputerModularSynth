#include "patch.h"
#include <cstring>

Patch::Patch() {
    memset(static_cast<void*>(this), 0, sizeof(*this));
    clear();
}

void Patch::clear() {
    memset(static_cast<void*>(modules_), 0, sizeof(modules_));
    memset(static_cast<void*>(cables_), 0, sizeof(cables_));
    nModules_ = 0;
    nCables_ = 0;
    nextId = 1;
}

ModuleRec* Patch::find(uint16_t id) {
    for (int i = 0; i < nModules_; i++)
        if (modules_[i].id == id) return &modules_[i];
    return nullptr;
}

const ModuleRec* Patch::find(uint16_t id) const {
    for (int i = 0; i < nModules_; i++)
        if (modules_[i].id == id) return &modules_[i];
    return nullptr;
}

const ModuleRec* Patch::findAt(uint8_t page, uint8_t cell) const {
    for (int i = 0; i < nModules_; i++)
        if (modules_[i].page == page && modules_[i].cell == cell) return &modules_[i];
    return nullptr;
}

int Patch::countOfType(const ModuleTypeInfo& t) const {
    int n = 0;
    for (int i = 0; i < nModules_; i++)
        if (modules_[i].type == &t) n++;
    return n;
}

PatchError Patch::addModule(const ModuleTypeInfo& t, uint8_t page, uint8_t cell, uint16_t* outId) {
    const PatchError e = addModuleWithId(nextId, t, page, cell);
    if (e == PatchError::None && outId) *outId = modules_[nModules_ - 1].id;
    return e;
}

PatchError Patch::addModuleWithId(uint16_t id, const ModuleTypeInfo& t, uint8_t page, uint8_t cell) {
    if (page >= kPages || cell >= kCellsPerPage) return PatchError::BadPin;
    if (findAt(page, cell)) return PatchError::CellTaken;
    if (id > 0xFEFF) return PatchError::BadPin;  // nextId must never wrap
    if (find(id)) return PatchError::Duplicate;
    if (nModules_ >= kMaxModules) return PatchError::TooManyModules;
    if (t.maxInstances && countOfType(t) >= t.maxInstances) return PatchError::MaxInstances;
    ModuleRec& m = modules_[nModules_++];
    memset(static_cast<void*>(&m), 0, sizeof(m));
    m.id = id;
    if (id >= nextId) nextId = static_cast<uint16_t>(id + 1);
    m.type = &t;
    m.page = page;
    m.cell = cell;
    for (int i = 0; i < t.numParams && i < kMaxParams; i++) m.params[i] = t.params[i].def;
    for (int i = 0; i < kMaxOuts; i++) m.outAmount[i] = 1.f;
    return PatchError::None;
}

bool Patch::removeModule(uint16_t id) {
    int idx = -1;
    for (int i = 0; i < nModules_; i++)
        if (modules_[i].id == id) idx = i;
    if (idx < 0) return false;
    for (int i = idx; i + 1 < nModules_; i++) modules_[i] = modules_[i + 1];
    nModules_--;
    memset(static_cast<void*>(&modules_[nModules_]), 0, sizeof(ModuleRec));
    int w = 0;
    for (int i = 0; i < nCables_; i++)
        if (cables_[i].fromId != id && cables_[i].toId != id) cables_[w++] = cables_[i];
    memset(static_cast<void*>(&cables_[w]), 0, sizeof(CableRec) * (nCables_ - w));
    nCables_ = w;
    return true;
}

PatchError Patch::moveModule(uint16_t id, uint8_t page, uint8_t cell) {
    ModuleRec* m = find(id);
    if (!m || page >= kPages || cell >= kCellsPerPage) return PatchError::BadPin;
    const ModuleRec* at = findAt(page, cell);
    if (at && at != m) return PatchError::CellTaken;
    m->page = page;
    m->cell = cell;
    return PatchError::None;
}

PatchError Patch::addCable(uint16_t fromId, uint8_t fromOut, uint16_t toId, uint8_t toIn) {
    const ModuleRec* f = find(fromId);
    const ModuleRec* t = find(toId);
    if (!f || !t || fromOut >= f->type->numOuts || toIn >= t->type->numIns) return PatchError::BadPin;
    for (int i = 0; i < nCables_; i++)
        if (cables_[i].fromId == fromId && cables_[i].fromOut == fromOut && cables_[i].toId == toId &&
            cables_[i].toIn == toIn)
            return PatchError::Duplicate;
    if (nCables_ >= kMaxCables) return PatchError::TooManyCables;
    int used[kNumCableColors] = {};
    for (int i = 0; i < nCables_; i++) used[cables_[i].color % kNumCableColors]++;
    int best = 0;
    for (int c = 1; c < kNumCableColors; c++)
        if (used[c] < used[best]) best = c;
    CableRec& c = cables_[nCables_++];
    memset(static_cast<void*>(&c), 0, sizeof(c));
    c.fromId = fromId;
    c.fromOut = fromOut;
    c.toId = toId;
    c.toIn = toIn;
    c.color = static_cast<uint8_t>(best);
    return PatchError::None;
}

int Patch::removeCablesAtPin(uint16_t id, bool isOutput, uint8_t pin) {
    int w = 0;
    for (int i = 0; i < nCables_; i++) {
        const CableRec& c = cables_[i];
        bool hit = isOutput ? (c.fromId == id && c.fromOut == pin) : (c.toId == id && c.toIn == pin);
        if (!hit) cables_[w++] = c;
    }
    int removed = nCables_ - w;
    memset(static_cast<void*>(&cables_[w]), 0, sizeof(CableRec) * removed);
    nCables_ = w;
    return removed;
}
