#pragma once
#include <cstdint>
#include <type_traits>
#include "module.h"
#include "units.h"

struct ModuleRec {
    uint16_t id;
    const ModuleTypeInfo* type;
    uint8_t page, cell;  // cell = row*12 + col
    float params[kMaxParams];
    float outAmount[kMaxOuts];
};

struct CableRec {
    uint16_t fromId;
    uint8_t fromOut;
    uint16_t toId;
    uint8_t toIn;
    uint8_t color;
};

enum class PatchError : uint8_t { None, CellTaken, TooManyModules, TooManyCables, MaxInstances, Duplicate, BadPin };

class Patch {
   public:
    Patch();  // zeroes the whole object, so unused slots and padding compare equal with memcmp

    PatchError addModule(const ModuleTypeInfo& t, uint8_t page, uint8_t cell, uint16_t* outId);  // params = defaults, amounts = 1
    // Loader: keeps a stored id (Duplicate if taken) and raises nextId past it.
    PatchError addModuleWithId(uint16_t id, const ModuleTypeInfo&, uint8_t page, uint8_t cell);
    void setCableColor(int i, uint8_t color) { if (i >= 0 && i < nCables_) cables_[i].color = color; }
    bool removeModule(uint16_t id);  // also removes its cables
    PatchError moveModule(uint16_t id, uint8_t page, uint8_t cell);
    uint8_t nextCableColor() const;  // the colour addCable would pick
    PatchError addCable(uint16_t fromId, uint8_t fromOut, uint16_t toId, uint8_t toIn);  // picks colour
    int removeCablesAtPin(uint16_t id, bool isOutput, uint8_t pin);  // returns how many were removed
    ModuleRec* find(uint16_t id);
    const ModuleRec* find(uint16_t id) const;
    const ModuleRec* findAt(uint8_t page, uint8_t cell) const;
    int moduleCount() const { return nModules_; }
    const ModuleRec& moduleAt(int i) const { return modules_[i]; }
    int cableCount() const { return nCables_; }
    const CableRec& cableAt(int i) const { return cables_[i]; }
    int countOfType(const ModuleTypeInfo& t) const;
    void clear();

    uint16_t nextId;  // ids never reused within a session

   private:
    ModuleRec modules_[kMaxModules];
    CableRec cables_[kMaxCables];
    int nModules_, nCables_;
};

static_assert(std::is_trivially_copyable<Patch>::value, "undo snapshots Patch with =");
static_assert(sizeof(Patch) <= 3072, "Patch snapshot size budget");
