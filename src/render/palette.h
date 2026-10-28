#pragma once
#include <cstdint>

// Palette indices for the 4-bit sprite. Theme = swapping the RGB values in kStyleB.
enum Pal : uint8_t {
    PBg = 0, PStrip = 1, PText = 2, PDim = 3, PFaint = 4, PBorder = 5, PAccent = 6, PEmpty = 7,
    PCable0 = 8,  // 8..13: cable colours in patch order
    PSpare0 = 14, PSpare1 = 15,
};

// Style B (spec §6), 0xRRGGBB. PSpare0/1 = modulator / processor category bars (theme A only).
constexpr uint32_t kStyleB[16] = {
    0xc4c6b8, 0xb6b8aa, 0x1d1f1a, 0x5e6157, 0x9a9d90, 0x8f9286, 0xff4d1a, 0xadb0a2,
    0xffe14d, 0xff8fc8, 0x39d0ff, 0xff3d9a, 0xa77bff, 0xb6ff3b, 0x1d1f1a, 0x1d1f1a,
};
// Style A "OP-1 dark" (spec §6).
constexpr uint32_t kStyleA[16] = {
    0x0b0b0c, 0x101113, 0xece9e2, 0x7b7f85, 0x3a3d42, 0x2a2c30, 0xff6a2b, 0x16171a,
    0xffe14d, 0xff8fc8, 0x39d0ff, 0xff3d9a, 0xa77bff, 0xb6ff3b, 0x4c7dff, 0xcfcac0,
};
