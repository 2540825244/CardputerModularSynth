#pragma once
#include <cstdint>
#include "core/engine.h"
#include "core/patch.h"
#include "core/storage.h"
#include "ui/ui_state.h"

// Core-0 only. renderBegin allocates the 4-bit sprite; false if it could not be created.
bool renderBegin();
// Full redraw into the sprite, then pushSprite. Call at <= 30 fps.
void renderFrame(const UiState&, const Patch&, Engine&, Storage&, uint32_t nowMs, bool fnHeld);
void renderSetTheme(int theme);  // 0 = style B (grey LCD), 1 = style A (dark)
void renderSetBrightness(int level);  // 1..5
// Sprite buffer bytes, for the RAM report.
uint32_t renderSpriteBytes();
