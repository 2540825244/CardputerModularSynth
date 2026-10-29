#pragma once
#include "ui/key_input.h"

// TCA8418 keyboard, read by polling its INT level (no GPIO interrupt). Call after M5.begin.
bool keyboardBegin();
// Drains the TCA8418 queue when it holds events and returns the keys held down.
int keyboardPoll(KeyPos* out, int max);
