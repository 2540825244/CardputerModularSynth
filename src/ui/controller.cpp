#include "controller.h"
#include "core/transport.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <ArduinoJson.h>
#include "core/patch_io.h"
#include "core/presets.h"
#include "layout.h"
#include "views.h"

UiController::UiController(Patch& p, Engine& e, UndoStack& u, Storage& st)
    : patch_(p), engine_(e), undo_(u), storage_(st), s_{} {
    s_.view = View::Patch;
    s_.settings.sleep = 2;  // until loadSettings
    s_.selCell = -1;
    s_.modCell = -1;
    s_.moveFromId = -1;
    s_.placeType = -1;
    s_.fileSlot = -1;
    s_.pressCell = -1;
}

const ViewDef& viewDef(View v) {
    static const ViewDef kPatch{patchViewAction, false, true}, kModule{moduleViewAction, false, false},
        kPlace{placeViewAction, false, false}, kFile{fileViewAction, false, false};
    switch (v) {
        case View::Module: return kModule;
        case View::Place: return kPlace;
        case View::File: return kFile;
        case View::Patch: break;
    }
    return kPatch;
}

static bool sameKey(const KeyEvent& a, const KeyEvent& b) {
    return a.grid == b.grid && (a.grid ? a.col == b.col && a.row == b.row : a.side == b.side);
}

void UiController::handle(const KeyEvent& e, uint32_t nowMs) {
    now_ = nowMs;
    // A sleeping screen: the first key only wakes, and stays swallowed down to its Up.
    if (waking_ && sameKey(e, wakeKey_)) {
        if (e.type == KeyEvent::Up) waking_ = false;
        lastKeyMs_ = nowMs;
        return;
    }
    if (e.type == KeyEvent::Down && screen(nowMs) != Screen::On) {
        waking_ = true;
        wakeKey_ = e;
        lastKeyMs_ = nowMs;
        return;
    }
    lastKeyMs_ = nowMs;
    if (!e.grid && e.side == Side::Fn) fn_ = e.type != KeyEvent::Up;
    if (!e.grid && e.side == Side::Ctrl) ctrl_ = e.type != KeyEvent::Up;
    coalescedEdit_ = false;
    if (!e.grid) {
        const uint8_t bit = (uint8_t)(1u << (int)e.side);
        if (e.type == KeyEvent::Up) s_.sideHeld &= (uint8_t)~bit;
        else s_.sideHeld |= bit;
        if (e.type == KeyEvent::Down) s_.sideDownMs[(int)e.side] = nowMs;
    } else if (e.type == KeyEvent::Down) {
        s_.pressCell = (int8_t)cellOf(e);
        s_.pressMs = nowMs;
    }
    if (e.type == KeyEvent::Down) {
        // A second consecutive Esc cancels a carry and does nothing else; Del cancels it too.
        if (!e.grid && e.side == Side::Esc) {
            if (s_.carry.active && s_.escPresses >= 1) {
                s_.carry.active = false;
                s_.escPresses = 0;
                undo_.breakCoalescing();
                return;
            }
            if (s_.escPresses < 255) s_.escPresses++;
        } else {
            s_.escPresses = 0;
            if (!e.grid && e.side == Side::Del && s_.carry.active) {
                s_.carry.active = false;
                undo_.breakCoalescing();
                return;
            }
        }
    }
    if (auditioning && !e.grid && e.side == Side::Space && e.type == KeyEvent::Up) releaseAudition();
    if (pushId >= 0 && !e.grid && e.side == Side::Space && e.type == KeyEvent::Up) releasePush();
    // Gestures become intents in one place (gesture.h); the view current at each action answers
    // it. If an action changes the view, the rest of this event is dropped and keys still held
    // stay with the old view.
    const View before = s_.view;
    Action acts[2];
    const ViewDef& vd = viewDef(before);
    const int n = mapper_.map(e, fnHeld(), ctrlHeld() && vd.ctrlBlocksHold, vd.rawKeys || onPlayPage(), acts, nowMs);
    for (int i = 0; i < n && s_.view == before; i++) {
        // A lone fn tap drops a carried cable in every view (the fn slot is lit while carrying).
        if (acts[i].act == Act::SideTap && acts[i].key.side == Side::Fn && s_.carry.active) {
            s_.carry.active = false;
            undo_.breakCoalescing();
            continue;
        }
        if (vd.onAction(*this, acts[i])) {
            mapper_.consume(e);
            break;
        }
    }
    validate();
    if (s_.view != before) {
        mapper_.reset();
        s_.prevView = before;
        s_.viewMs = nowMs;
    }
    if (tilt_.active && s_.view != View::Module) tilt_.active = false;
    if (auditioning && s_.view != View::Patch) releaseAudition();
    if (pushId >= 0 && (s_.view != View::Module || s_.openId != pushId)) releasePush();
    // Anything but a coalescing param edit (cursor move, view change) ends the run. A key release
    // changes nothing, so repeated taps of one fine key still merge; tilt drags (Task 19) end
    // their run explicitly on release.
    if (!coalescedEdit_ && e.type != KeyEvent::Up) undo_.breakCoalescing();
}

bool UiController::onPlayPage() const {
    if (s_.view != View::Module) return false;
    const ModuleRec* m = patch_.find(s_.openId);
    return m && modulePageKind(*m->type, s_.modPage) == PageKind::Play;
}

KeysModule* UiController::keysInstance() {
    for (int i = 0; i < patch_.moduleCount(); i++)
        if (patch_.moduleAt(i).type == &kKeysType)
            return static_cast<KeysModule*>(engine_.instance(patch_.moduleAt(i).id));
    return nullptr;
}

KitModule* UiController::kitInstance() {
    for (int i = 0; i < patch_.moduleCount(); i++)
        if (patch_.moduleAt(i).type == &kKitType)  // bypassed: no pass (it would start once enabled)
            return patch_.moduleAt(i).bypass ? nullptr : static_cast<KitModule*>(engine_.instance(patch_.moduleAt(i).id));
    return nullptr;
}

void UiController::pushInput(uint16_t id, uint8_t in) {
    releasePush();
    if (Module* m = engine_.instance(id)) {
        m->pushInput(in, true);
        pushId = id;
        pushIn = in;
    }
}

void UiController::releasePush() {
    if (pushId >= 0)
        if (Module* m = engine_.instance((uint16_t)pushId)) m->pushInput(pushIn, false);
    pushId = -1;
}

void UiController::releaseAudition() {
    auditioning = false;
    if (KeysModule* k = keysInstance()) k->gateOff();
}

void UiController::validate() {
    if (s_.carry.active && !patch_.find(s_.carry.id)) s_.carry.active = false;
    if (s_.view == View::Module && !patch_.find(s_.openId)) {
        s_.view = View::Patch;
        notes.clear();
        s_.keysHeld = 0;
    }
    if (s_.moveFromId >= 0 && !patch_.find((uint16_t)s_.moveFromId)) s_.moveFromId = -1;
    if (s_.latchedBit >= 0 && !keysInstance()) {  // KEYS deleted, or undone away
        s_.latchedBit = -1;
        s_.keysHeld = 0;
    }
}

void UiController::tick(uint32_t nowMs) {
    if (s_.toast[0] && (int32_t)(nowMs - s_.toastUntilMs) >= 0) s_.toast[0] = 0;
    now_ = nowMs;
    engine_.collect();
    // Measured load near the deadline (GRD switched to a heavy engine after it was placed): say so
    // once per episode.
    if (engine_.cpuLoad() < 0.95f) {
        cpuFullSinceMs_ = 0;
        cpuFullWarned_ = false;
    } else if (!cpuFullSinceMs_) {
        cpuFullSinceMs_ = nowMs | 1u;
    } else if (!cpuFullWarned_ && nowMs - cpuFullSinceMs_ >= 500) {
        toast("CPU FULL", nowMs);
        cpuFullWarned_ = true;
    }
    // A module in the patch that could not be created (no heap block) makes no sound: say so.
    // 300 ms first: a DLY swap is missing until the audio thread lets go of the old one.
    if (engine_.missing() == 0) {
        missingSinceMs_ = 0;
        missingWarned_ = false;
    } else if (!missingSinceMs_) {
        missingSinceMs_ = nowMs | 1u;
    } else if (!missingWarned_ && nowMs - missingSinceMs_ >= 300) {
        toast("NO RAM", nowMs);
        missingWarned_ = true;
    }
    if (tilt_.active) return;  // no autosave mid-gesture
    if (!s_.dirty || (int32_t)(nowMs - s_.lastEditMs) < 2000) return;
    if (autosaveBackoff_ && (int32_t)(nowMs - autosaveRetryMs_) < 0) return;
    const std::string json = patchToJson(patch_);
    if (json.empty()) {  // out of heap: keep dirty, try again later
        // Said again every 30 s: edits since the last good save are lost at power-off.
        if (!autosaveWarned_ || (int32_t)(nowMs - autosaveWarnMs_) >= 30000) {
            toast("NOT SAVED: NO RAM", nowMs);
            autosaveWarnMs_ = nowMs;
        }
        autosaveWarned_ = autosaveBackoff_ = true;
        autosaveRetryMs_ = nowMs + 5000;
        return;
    }
    if (!patchFits(json)) {  // never write a file the loader would refuse
        if (!autosaveWarned_) toast("PATCH TOO BIG", nowMs);
        autosaveWarned_ = autosaveBackoff_ = true;
        autosaveRetryMs_ = nowMs + 30000;
        return;
    }
    if (storage_.available() && storage_.write(slotPath(0).c_str(), json)) {
        s_.dirty = false;
        autosaveBackoff_ = autosaveWarned_ = false;
        return;
    }
    if (!autosaveWarned_) toast(storage_.available() ? "AUTOSAVE FAILED" : "NO SD", nowMs);
    autosaveWarned_ = true;
    autosaveBackoff_ = true;
    autosaveRetryMs_ = nowMs + 30000;
}

bool UiController::loadFile(const char* path, bool keepUndo, char* msg, size_t msgSize) {
    std::string text;
    if (!storage_.available()) {
        snprintf(msg, msgSize, "NO SD");
        return false;
    }
    if (!storage_.read(path, text)) {
        snprintf(msg, msgSize, "NO FILE");
        return false;
    }
    return loadJson(text, keepUndo, msg, msgSize);
}

bool UiController::loadJson(const std::string& text, bool keepUndo, char* msg, size_t msgSize) {
    std::unique_ptr<Patch> loaded(new Patch());  // too big for the stack
    const LoadReport r = patchFromJson(text, *loaded, &engine_);
    if (r.error != LoadError::None) {
        snprintf(msg, msgSize, r.error == LoadError::NewerFormat ? "NEWER FW"
                               : r.error == LoadError::NoMemory  ? "NO RAM"
                                                                 : "BAD FILE");
        return false;
    }
    if (keepUndo) undo_.push(patch_, 0);
    patch_ = *loaded;
    engine_.apply(patch_);
    transport::play();  // a loaded patch plays from step 1
    // Ids may now name different modules: drop every id-bound or position-bound UI reference.
    s_.carry.active = false;
    s_.view = View::Patch;
    s_.openId = 0;
    s_.moveFromId = -1;
    s_.selCell = -1;
    s_.modCell = -1;
    s_.modPage = 0;
    notes.clear();
    s_.keysHeld = 0;
    s_.latchedBit = -1;
    if (keepUndo) {
        s_.dirty = true;
        s_.lastEditMs = now_;
    } else {
        s_.dirty = false;
    }
    validate();
    // Budget skips name the type: a slot that loaded at boot can skip a module once its measured
    // cost is known.
    if (r.skippedModules > 0 && r.skippedType) snprintf(msg, msgSize, "SKIPPED %d %s", r.skippedModules, r.skippedType);
    else if (r.skippedModules > 0) snprintf(msg, msgSize, "SKIPPED %d", r.skippedModules);
    else snprintf(msg, msgSize, "LOADED");
    return true;
}

bool UiController::loadPreset(int i, bool keepUndo, char* msg, size_t msgSize) {
    int n = 0;
    const Preset* ps = presets(&n);
    if (i < 0 || i >= n) {
        snprintf(msg, msgSize, "NO PRESET");
        return false;
    }
    return loadJson(ps[i].json, keepUndo, msg, msgSize);
}

namespace {
void doClear(Patch& p, void*) {
    // Keep nextId: a new module must not reuse an id an undo could bring back (Engine::apply
    // would hand it the old module's DSP state).
    const uint16_t next = p.nextId;
    p.clear();
    p.nextId = next;
}
}  // namespace

void UiController::newPatch() {
    edit(doClear, nullptr);
    transport::play();
    s_.selCell = -1;
    s_.moveFromId = -1;
    s_.carry.active = false;
    s_.view = View::Patch;
    toast("NEW PATCH", now_);
}

bool UiController::bootLoad() {
    loadSettings();
    if (loadAutosave()) return true;
    char msg[24];
    return loadPreset(0, false, msg, sizeof msg);
}

uint32_t UiController::holdMs() const {
    static const uint32_t kHold[] = {300, 400, 600};
    return kHold[s_.settings.hold < 3 ? s_.settings.hold : 1];
}

Screen UiController::screen(uint32_t nowMs) const { return screenFor(nowMs - lastKeyMs_, s_.settings.sleep); }

void UiController::loadSettings() {
    s_.settings = {0, 4, 1, 2};
    std::string text;
    if (storage_.available() && storage_.read(kSettingsPath, text)) {
        JsonDocument doc;
        if (deserializeJson(doc, text) == DeserializationError::Ok) {
            const int theme = doc["theme"] | 0, bright = doc["bright"] | 4, hold = doc["hold"] | 400;
            s_.settings.theme = theme ? 1 : 0;
            s_.settings.brightness = (uint8_t)(bright < 1 ? 1 : (bright > 5 ? 5 : bright));
            s_.settings.hold = hold <= 300 ? 0 : (hold >= 600 ? 2 : 1);
            const int sleep = doc["sleep"] | 2;  // minutes
            s_.settings.sleep = sleep <= 0 ? 0 : sleep <= 1 ? 1 : sleep <= 2 ? 2 : sleep <= 5 ? 3 : 4;
        }
    }
    s_.settingsGen++;
}

void UiController::saveSettings() {
    s_.settingsGen++;
    JsonDocument doc;
    doc["theme"] = s_.settings.theme;
    doc["bright"] = s_.settings.brightness;
    doc["hold"] = holdMs();
    doc["sleep"] = sleepAfterMs(s_.settings.sleep) / 60000u;
    std::string out;
    serializeJson(doc, out);
    if (storage_.available()) storage_.write(kSettingsPath, out);
}

bool UiController::loadAutosave() {
    const std::string path = slotPath(0);
    if (!storage_.exists(path.c_str())) return false;
    char msg[24];
    return loadFile(path.c_str(), false, msg, sizeof msg);
}

namespace {
struct TiltEdit {
    uint16_t id;
    uint8_t idx;
    float value;
};
void doTilt(Patch& p, void* c) {
    const TiltEdit* t = static_cast<TiltEdit*>(c);
    if (ModuleRec* m = p.find(t->id)) m->params[t->idx] = t->value;
}
}  // namespace

void UiController::beginTilt(uint16_t id, uint8_t idx) {
    const ModuleRec* m = patch_.find(id);
    if (!m || idx >= m->type->numParams) return;
    tilt_ = {true, id, idx, paramToNorm(m->type->params[idx], m->params[idx]), false};
    tiltGen_++;
}

void UiController::endTilt() {
    if (!tilt_.active) return;
    tilt_.active = false;
    if (s_.dirty) s_.lastEditMs = now_;  // autosave debounce counts from the release
    undo_.breakCoalescing();
}

void UiController::tilt(float degrees) {
    if (!tilt_.active) return;
    const ModuleRec* m = patch_.find(tilt_.id);
    if (!m) {
        tilt_.active = false;
        return;
    }
    if (!tilt_.moved && fabsf(degrees) <= 1.f) return;
    tilt_.moved = true;
    const ParamSpec& sp = m->type->params[tilt_.idx];
    const float v = paramFromNorm(sp, tilt_.startNorm + degrees / 60.f);
    if (v == m->params[tilt_.idx]) return;
    TiltEdit t{tilt_.id, tilt_.idx, v};
    editValue(doTilt, &t, paramKey(tilt_.id, tilt_.idx, false));
    engine_.setParam(tilt_.id, tilt_.idx, v);
}

bool UiController::edit(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey) {
    const uint32_t before = kitCycles();
    undo_.push(patch_, coalesceKey);
    fn(patch_, ctx);
    engine_.apply(patch_);
    validate();  // the edit may have removed what the UI points at (KEYS with a latched note)
    s_.dirty = true;
    s_.lastEditMs = now_;
    if (coalesceKey) coalescedEdit_ = true;
    warnKitCpu(before);
    return true;
}

uint32_t UiController::kitCycles() const {
    return patch_.countOfType(kKitType) ? engine_.patchCycles(patch_) : 0;
}

// With a KIT, a cable or a step can raise the worst step past the budget; one overloaded step is
// too short for the measured-load warning, so say it at the edit. Only an edit that raised the
// estimate says it: the estimate over-counts, and a patch already over it that plays fine should
// not warn at every GAP nudge (owner, 2026-10-29).
void UiController::warnKitCpu(uint32_t before) {
    const bool showing = !strcmp(s_.toast, "CPU FULL") && (int32_t)(now_ - s_.toastUntilMs) < 0;
    const uint32_t after = kitCycles();
    if (!showing && after > before && after > kCpuBudget)
        toast("CPU FULL", now_);  // not re-armed while it shows (a tilt sweep edits at 50 Hz)
}

bool UiController::editValue(void (*fn)(Patch&, void*), void* ctx, uint32_t coalesceKey) {
    const uint32_t before = kitCycles();
    undo_.push(patch_, coalesceKey);
    fn(patch_, ctx);
    s_.dirty = true;
    s_.lastEditMs = now_;
    coalescedEdit_ = true;
    warnKitCpu(before);
    return true;
}

bool UiController::undo() {
    if (!undo_.undo(patch_)) return false;
    engine_.apply(patch_);
    s_.dirty = true;
    s_.lastEditMs = now_;
    return true;
}

bool UiController::redo() {
    if (!undo_.redo(patch_)) return false;
    engine_.apply(patch_);
    s_.dirty = true;
    s_.lastEditMs = now_;
    return true;
}

bool UiController::undoOrRedo() {
    const bool r = fnHeld();
    if (r ? redo() : undo()) return true;
    toast(r ? "NOTHING TO REDO" : "NOTHING TO UNDO", now_);
    return false;
}

void UiController::toast(const char* msg, uint32_t nowMs) {
    strncpy(s_.toast, msg, sizeof(s_.toast) - 1);
    s_.toast[sizeof(s_.toast) - 1] = 0;
    s_.toastUntilMs = nowMs + 1500;
}
