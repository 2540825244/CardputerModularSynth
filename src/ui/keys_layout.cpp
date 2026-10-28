#include "keys_layout.h"
#include "core/scales.h"

namespace {
const char* const kNames[] = {"CHR", "SCL", "PNO", "BAS", "4TH", "PAD"};
const char* const kLong[] = {"CHROMATIC", "SCALE", "PIANO", "BASS FRET", "FOURTHS", "DRUM PADS"};

// PNO: white keys C D E F G A B from col 0; a black key sits above the gap right of its white key.
int piano(int base, bool blackRow, int col) {
    static const int kWhite[7] = {0, 2, 4, 5, 7, 9, 11};
    const int oct = col / 7, w = kWhite[col % 7];
    if (!blackRow) return base + 12 * oct + w;
    if (w == 4 || w == 11) return -1;  // no black key after E or B
    return base + 12 * oct + w + 1;
}
}  // namespace

const char* keysLayoutName(int l) { return kNames[l < 0 || l >= kLayCount ? 0 : l]; }
const char* keysLayoutLongName(int l) { return kLong[l < 0 || l >= kLayCount ? 0 : l]; }

int keysNoteAt(int layout, int scale, int root, int row, int col) {
    if (row < 0 || row > 3 || col < 0 || col > 11) return -1;
    const int up = 3 - row;  // rows from the bottom
    switch (layout) {
        case kLayScl: {  // each row an octave of the scale; columns walk its degrees
            const int n = scaleSize(scale);
            return 36 + root + scaleDegree(scale, up * n + col);
        }
        case kLayPno:  // two pianos: rows 3/2 from C3, rows 1/0 from C4
            return piano(up < 2 ? 48 : 60, up % 2 == 1, col);
        case kLayBas: {  // strings E1 A1 D2 G2, col = fret
            static const int kOpen[4] = {28, 33, 38, 43};
            return kOpen[up] + col;
        }
        case kLay4th: return 36 + 5 * up + col;
        case kLayPad: return 36 + (col / 4) * 16 + up * 4 + col % 4;
        default: return 36 + 12 * up + col;  // chromatic, 4 octaves
    }
}
