#pragma once
#include <cstdint>

enum class View : uint8_t { Patch, Module, Place, File, KeysPlay, KeysSettings };

struct Carry {
    bool active;
    uint16_t id;
    bool fromOutput;
    uint8_t pin;
    uint8_t color;
};

struct UiState {
    View view;
    uint8_t page;       // 0..2
    int8_t selCell;     // patch view selection on the current page, -1 none
    uint16_t openId;    // module whose view is open
    int8_t modCell;     // module-view selected cell 0..47, -1 none
    int16_t moveFromId; // ctrl-move pending: id, -1 none
    uint8_t placeCell;  // target of the place menu
    int8_t placeType;   // index into allTypes() whose details are shown, -1 = tile grid
    Carry carry;
    uint8_t escPresses;
    char toast[24];
    uint32_t toastUntilMs;
    bool fileSaveMode;
    bool dirty;
    uint32_t lastEditMs;
    uint64_t keysHeld;  // KeysPlay: bit (3-row)*12+col per held note key
};
