#pragma once

// Scales shared by QNT, SEQ and the KEYS layouts. A scale is a 12-bit mask of semitones above the
// root (bit 0 = root).
int scaleCount();
const char* const* scaleNames();  // stepped-param enum names, <= 3 chars (= kScaleNames)
extern const char* const kScaleNames[13];
unsigned scaleMask(int scale);
int scaleSize(int scale);           // notes per octave
int scaleDegree(int scale, int k);  // semitones above the root of degree k (k may be negative/any)
// Nearest in-scale semitone (ties go up) to `volts` (1 V/oct, 0 V = C4), returned in volts.
float quantize(float volts, int scale, int root);
extern const char* const kNoteNames[12];  // "C", "C#", ...
