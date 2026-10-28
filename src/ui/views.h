#pragma once
#include "controller.h"
#include "gesture.h"

// Per-view intent handlers, called by UiController::handle through viewDef(). A handler returns
// true to swallow the rest of the event's gesture: the remaining actions of this event and any
// later hold/release of the same key.
bool patchViewAction(UiController&, const Action&);
bool placeViewAction(UiController&, const Action&);
bool moduleViewAction(UiController&, const Action&);
bool keysPlayAction(UiController&, const Action&);  // a KEYS Play page (raw keys)
void keysAllNotesOff(UiController&);
void keysLeavePage(UiController&);  // esc / tab off the Play page: all off unless latched
int keysNoteFor(const ModuleRec&, int row, int col);  // the play page's note under a cell, -1 none
void flipPage(UiController&, const ModuleRec&, int dir);  // module view: tab / fn+tab
bool fileViewAction(UiController&, const Action&);

struct ViewDef {
    bool (*onAction)(UiController&, const Action&);
    bool rawKeys;         // true: every key arrives as Act::Key (the Keys play view plays notes)
    bool ctrlBlocksHold;  // true: ctrl held turns a grid hold's do-it off (patch view: ctrl = move)
};
const ViewDef& viewDef(View);

inline int cellOf(const KeyEvent& e) { return e.row * kGridCols + e.col; }
