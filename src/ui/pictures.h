#pragma once
#include "core/module.h"
#include "core/patch.h"
#include "gfx.h"
#include "ui_state.h"

// Module pictures (v1.4): one painter per type, drawn over the Drawing cells of a page whose
// drawing is Drawing::Picture, as a place-menu thumbnail, and (some types) as the patch-tile glyph.
// Painters are size-agnostic and compute only from the record's params plus, when live, the
// instance's 2 monitor slots; the selected param's part draws in the accent (emphasis).
struct PicCtx {
    Gfx& g;
    const ModuleRec& m;
    const Module* inst;  // null: static only (thumbnail, missing instance)
    int page;
    int sel;  // selected param index, -1 none
    int x, y, w, h;
    bool small;  // thumbnail / glyph: no text, no grid
};

bool hasPicture(const ModuleTypeInfo& t);
bool hasPainter(const ModuleTypeInfo& t);  // a painter for t's id exists
void drawPicture(const PicCtx& c);
// Patch tile glyph (about 13 x 11) that follows the params; false: the type keeps its icon.
bool drawTileGlyph(Gfx& g, const ModuleRec& m, int x, int y, int w, int h, uint8_t color);
// A param's cell label (<= 3 chars) and strip name as the module view shows them: GRD's HAR / TIM
// / MOR take the engine's own names; everything else is its ParamSpec (label, key upper-cased).
const char* paramLabel(const ModuleRec& m, int idx);
const char* paramName(const ModuleRec& m, int idx, char* buf, int len);
// v1.5: ROL's ROLL page over the whole grid box: the number row's functions, then the roll with
// the cursor (UiState::rollCursor / rollLen) and the playing step (inst's monitor 0).
void drawRollPage(Gfx& g, const ModuleRec& m, const Module* inst, const UiState& s, int x, int y, int w, int h);
// A record of type t with every param at its default (thumbnails).
ModuleRec defaultRecord(const ModuleTypeInfo& t);

// ---- picture maths, shared with the tests ----
namespace pic {
extern unsigned gNonFinite;  // non-finite values painters computed (see pictures_internal.h)
// "C3 +12" from a pitch in semitones relative to C4 (MIDI 60); cents rounded, -50..+49.
void noteName(float semisFromC4, char* out, int len);
// One cycle of a basic wave at phase t (any real), -1..1. kind: 0 sin, 1 tri, 2 saw, 3 square.
float basicWave(int kind, float t);
// GRD: the engine's own name for HAR / TIM / MOR (knob 0..2) and its long name.
const char* grdKnobLabel(int engine, int knob);
const char* grdKnobName(int engine, int knob);
const char* grdEngineName(int engine);
}  // namespace pic
