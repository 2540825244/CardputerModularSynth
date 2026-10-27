#pragma once
#include "ui/key_input.h"

// Drains the TCA8418 queue (one event per M5Cardputer.update()) and returns the pressed keys.
int keyboardPoll(KeyPos* out, int max);
