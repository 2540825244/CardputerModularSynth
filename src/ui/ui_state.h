#pragma once
#include <cstdint>

enum class View : uint8_t { Patch, Module, Place, File };

struct Carry {
    bool active;
    uint16_t id;
    bool fromOutput;
    uint8_t pin;
    uint8_t color;
};

struct Settings {
    uint8_t theme;       // 0 style B, 1 style A
    uint8_t brightness;  // 1..5
    uint8_t hold;        // 0 = 300, 1 = 400, 2 = 600 ms
};

struct UiState {
    View view;
    uint8_t page;       // 0..2
    int8_t selCell;     // patch view selection on the current page, -1 none
    uint16_t openId;    // module whose view is open
    int8_t modCell;     // module-view selected cell 0..47, -1 none
    uint8_t modPage;    // module-view page (layout.h)
    int16_t moveFromId; // ctrl-move pending: id, -1 none
    uint8_t placeCell;  // target of the place menu
    int8_t placeType;   // index into allTypes() whose details are shown, -1 = tile grid
    uint8_t placePage;  // place menu: category page (kept between visits)
    Carry carry;
    uint8_t escPresses;
    char toast[24];
    uint32_t toastUntilMs;
    uint8_t filePage;   // file menu: 0 LOAD, 1 SAVE, 2 PRESETS, 3 SYSTEM
    int8_t fileSlot;    // file menu: selected cell 0..47 on the page, -1 none
    Settings settings;
    uint32_t settingsGen;  // bumps on every settings change (main applies theme, brightness, hold)
    bool dirty;
    uint32_t lastEditMs;
    uint64_t keysHeld;  // KEYS play page: bit (3-row)*12+col per held note key
    int8_t latchedBit = -1;  // KEYS latch: the held note's grid bit, -1 = none
    // Key feedback for the renderer.
    uint8_t sideHeld;          // bit (int)Side per side key held now
    uint32_t sideDownMs[8];    // last Down of each side key
    int8_t pressCell;          // last grid Down (row*12+col), -1 none
    uint32_t pressMs;
    uint32_t viewMs;           // last view change
    View prevView;             // the view before the last change
};
