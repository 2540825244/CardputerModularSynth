#pragma once

// KEYS play-page note layouts over the whole 4x12 grid (row 3 = bottom = lowest). The KEYS module
// adds its OCT param on top. Pure, natively tested.
enum KeysLayout { kLayChr, kLayScl, kLayPno, kLayBas, kLay4th, kLayPad, kLayCount };
int keysNoteAt(int layout, int scale, int root, int row, int col);  // MIDI note, -1 = silent cell
const char* keysLayoutName(int layout);                             // "CHR", "SCL", ...
const char* keysLayoutLongName(int layout);                         // for the toast
