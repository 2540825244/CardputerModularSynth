#include "scales.h"
#include <cmath>

const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const char* const kScaleNames[13] = {"CHR", "MAJ", "MIN", "DOR", "PHR", "LYD", "MIX", "LOC", "HMN", "PN+", "PN-", "BLU", "WHL"};

namespace {
const unsigned kMasks[] = {
    0xFFF,                                              // chromatic
    0b101010110101, 0b010110101101, 0b011010101101,    // major, natural minor, dorian
    0b010110101011, 0b101011010101, 0b011010110101,    // phrygian, lydian, mixolydian
    0b010101101011, 0b100110101101,                    // locrian, harmonic minor
    0b001010010101, 0b010010101001,                    // major / minor pentatonic
    0b010011101001, 0b010101010101,                    // blues, whole tone
};
constexpr int kCount = sizeof(kMasks) / sizeof(kMasks[0]);
}  // namespace

int scaleCount() { return kCount; }
const char* const* scaleNames() { return kScaleNames; }
unsigned scaleMask(int s) { return kMasks[s < 0 || s >= kCount ? 0 : s]; }

int scaleSize(int s) {
    int n = 0;
    for (int i = 0; i < 12; i++) n += (scaleMask(s) >> i) & 1;
    return n;
}

int scaleDegree(int s, int k) {
    const int n = scaleSize(s);
    int oct = k >= 0 ? k / n : -((-k + n - 1) / n);
    int d = k - oct * n;
    const unsigned m = scaleMask(s);
    for (int i = 0; i < 12; i++)
        if ((m >> i) & 1) {
            if (d == 0) return oct * 12 + i;
            d--;
        }
    return oct * 12;
}

float quantize(float volts, int scale, int root) {
    const unsigned m = scaleMask(scale);
    const float semi = volts * 12.f - (float)root;
    int base = (int)semi;  // floorf without the libm call (|semi| is small: volts are clamped)
    if ((float)base > semi) base--;
    int best = base;
    float bestD = 1e9f;
    int pc = ((base - 6) % 12 + 12) % 12;  // pitch class of c, stepped along instead of two %
    for (int c = base - 6; c <= base + 7; c++, pc = pc == 11 ? 0 : pc + 1) {
        if (!((m >> pc) & 1)) continue;
        const float d = fabsf((float)c - semi);
        if (d < bestD - 1e-6f || (fabsf(d - bestD) < 1e-6f && c > best)) {
            bestD = d;
            best = c;
        }
    }
    return (float)(best + root) / 12.f;
}
