#include <M5Cardputer.h>
#include <esp_heap_caps.h>
#include "core/engine.h"
#include "core/patch.h"
#include "core/undo.h"
#include "hw/audio_out.h"
#include "hw/keyboard.h"
#include "hw/sd_storage.h"
#include "render/renderer.h"
#include "ui/controller.h"
#include "ui/key_input.h"

// Big objects are statics, never on a task stack.
static Patch gPatch;
static UndoStack gUndo;
static SdStorage gSd;
static Engine gEngine([]() -> uint32_t { return heap_caps_get_free_size(MALLOC_CAP_8BIT); });
static UiController gUi(gPatch, gEngine, gUndo, gSd);
static KeyDecoder gDecoder;

static void printHeap(const char* tag) {
    Serial.printf("[heap %s] free=%u largest=%u min=%u\n", tag, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), (unsigned)ESP.getMinFreeHeap());
}

// Keyboard, codec I2C, SD and the UI all live on core 0; core 1 only runs audio.
static void uiTask(void*) {
    if (audioI2sError() == 0) codecBegin();
    uint32_t lastStatus = 0, lastDraw = 0;
    bool steadyPrinted = false;
    KeyPos pressed[16];
    KeyEvent ev[16];
    for (;;) {
        const uint32_t now = millis();
        const int n = keyboardPoll(pressed, 16);
        int ne = 0;
        gDecoder.update(pressed, n, now, ev, &ne, 16);
        for (int i = 0; i < ne; i++) gUi.handle(ev[i], now);
        gUi.tick(now);
        if (now - lastDraw >= 33) {  // <= 30 fps, core 0 only
            lastDraw = now;
            renderFrame(gUi.state(), gPatch, gEngine, gSd, now);
        }
        if (now - lastStatus >= 500) {
            lastStatus = now;
            const UiState& s = gUi.state();
            Serial.printf("view=%d page=%d cpu=%.1f%% free=%u largest=%u i2s=%d codec=%d\n", (int)s.view, (int)s.page,
                          gEngine.cpuLoad() * 100.f, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), audioI2sError(),
                          codecFailures());
        }
        if (!steadyPrinted && now > 5000) {
            steadyPrinted = true;
            printHeap("steady");
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false;  // we own the codec
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    printHeap("boot");

    M5Cardputer.Display.setRotation(1);
    if (!renderBegin()) Serial.println("renderBegin FAILED (sprite)");
    printHeap("sprite");
    gSd.begin();
    gUi.setDecoder(&gDecoder);
    gUi.loadAutosave();
    if (!audioBegin(gEngine)) Serial.println("audioBegin FAILED");
    printHeap("init");
    xTaskCreatePinnedToCore(uiTask, "ui", 12288, nullptr, 1, nullptr, 0);
    vTaskDelete(nullptr);  // loop() never runs
}

void loop() {}
