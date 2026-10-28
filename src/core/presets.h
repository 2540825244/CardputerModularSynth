#pragma once

// Factory patches compiled into the firmware (generated: tools/presets.py).
struct Preset {
    const char* name;  // <= 6 chars
    const char* json;  // patch file text, format 1
};
const Preset* presets(int* count);
