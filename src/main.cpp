#include <M5Cardputer.h>
#include <esp_heap_caps.h>
#include <xtensa/hal.h>
#include "core/engine.h"
#include "core/modules/grd.h"
#include "core/modules/tilt.h"
#include "core/patch.h"
#include "core/registry.h"
#include "core/undo.h"
#include "hw/audio_out.h"
#include "hw/keyboard.h"
#include "hw/sd_storage.h"
#include "hw/tilt.h"
#include "render/renderer.h"
#include "ui/controller.h"
#include "ui/key_input.h"

// Big objects are statics, never on a task stack.
static Patch gPatch;
static UndoStack gUndo;
static SdStorage gSd;
static Engine gEngine([]() -> uint32_t { return heap_caps_get_free_size(MALLOC_CAP_8BIT); });
static uint32_t cycleCount() { return xthal_get_ccount(); }
static uint32_t largestBlock() { return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT); }
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
    uint32_t lastTiltGen = 0, settingsGen = 0;
    bool inputPending = false;  // redraw soon after a key, not at the next 30 fps tick
    float rollAtStart = 0.f;
    KeyPos pressed[16];
    KeyEvent ev[16];
    for (;;) {
        const uint32_t now = millis();
        const int n = keyboardPoll(pressed, 16);
        int ne = 0;
        gDecoder.update(pressed, n, now, ev, &ne, 16);
        for (int i = 0; i < ne; i++) gUi.handle(ev[i], now);
        if (ne > 0) inputPending = true;
        tiltPoll(now);
        if (tiltValid()) tiltPublish(tiltAngles().roll, tiltAngles().pitch);  // TILT modules
        if (gUi.tiltActive() && tiltValid()) {
            const float roll = tiltAngles().roll;  // within +-90, no wrap
            if (gUi.tiltGeneration() != lastTiltGen) {
                lastTiltGen = gUi.tiltGeneration();
                rollAtStart = roll;
            }
            gUi.tilt(roll - rollAtStart);
        }
        gUi.tick(now);
        if (gUi.state().settingsGen != settingsGen) {  // theme, brightness, hold time
            settingsGen = gUi.state().settingsGen;
            renderSetTheme(gUi.state().settings.theme);
            renderSetBrightness(gUi.state().settings.brightness);
            gDecoder.setHoldMs(gUi.holdMs());
        }
        if (now - lastDraw >= 33 || (inputPending && now - lastDraw >= 12)) {  // core 0 only
            lastDraw = now;
            inputPending = false;
            renderFrame(gUi.state(), gPatch, gEngine, gSd, now, gUi.fnHeld());
        }
        if (now - lastStatus >= 500) {
            lastStatus = now;
            const UiState& s = gUi.state();
            Serial.printf("view=%d page=%d cpu=%.1f%% free=%u largest=%u i2s=%d codec=%d stackHw ui=%u audio=%u\n", (int)s.view, (int)s.page,
                          gEngine.cpuLoad() * 100.f, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), audioI2sError(),
                          codecFailures(), (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                          (unsigned)audioStackHighWater());
        }
        if (gEngine.costsChanged()) {  // measured cycles/sample per type: the numbers to put in cpuCycles
            int n = 0;
            const ModuleTypeInfo* const* types = allTypes(&n);
            Serial.print("cost");
            for (int i = 0; i < n; i++)
                if (const uint32_t c = gEngine.measuredCost(*types[i]))
                    Serial.printf(" %s=%u/%u", types[i]->id, (unsigned)c, (unsigned)types[i]->cpuCycles);
            Serial.println();
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
    Serial.setTxTimeoutMs(0);  // never stall the UI task when no host is reading
    printHeap("boot");

    M5Cardputer.Display.setRotation(1);
    if (!renderBegin()) Serial.println("renderBegin FAILED (sprite)");
    printHeap("sprite");
    gSd.begin();
    gSd.selfTest();
    printHeap("sd");
    gUi.setDecoder(&gDecoder);
    gUi.bootLoad();
    gEngine.setCycleCounter(cycleCount);
    gEngine.setLargestBlock(largestBlock);
    if (!audioBegin(gEngine)) Serial.println("audioBegin FAILED");
    printHeap("init");
    Serial.printf("[grd] needs %u B: one block of %u + the rest\n", (unsigned)kGrdType.ramBytes,
                  (unsigned)kGrdType.largestAlloc);
    xTaskCreatePinnedToCore(uiTask, "ui", 12288, nullptr, 1, nullptr, 0);
    vTaskDelete(nullptr);  // loop() never runs
}

void loop() {}
