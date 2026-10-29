#pragma once
#include "../biquad.h"
#include "../module.h"

// v1.4 modules: ARP (arpeggiator), CLP (handclap), PRC (tom / conga / cowbell / clave / rim),
// VOW (vowel formant filter), EQ3 (3-band EQ).
extern const ModuleTypeInfo kArpType, kClpType, kPrcType, kVowType, kEq3Type;

// ARP: the pattern's notes in semitones above the root, for CHD (0..6), MOD (0 UP, 1 DN, 2 UPDN,
// 3 RND: the UP order), OCT (1..3). Returns the count (<= 24).
int arpPattern(int chord, int mode, int octaves, int* semis, int max);
// VOW: F1..F3 in Hz at a vowel position 0..4 (A E I O U, linear between), before SHF.
void vowFormants(float vowel, float* f3);
// EQ3: its three biquads (low shelf 200 Hz, peak at midHz, high shelf 4 kHz) at 48 kHz.
void eq3Design(float lowDb, float midDb, float midHz, float highDb, Biquad* b3);
// PRC: each mode's base pitch (Hz) and its shape for DEC 0..1: pitch sweep depth, body decay (ms),
// click / fast-part decay (ms, 0 = none).
extern const float kPrcBaseHz[5];
void prcShape(int mode, float decay, float* sweep, float* bodyMs, float* clickMs);
