#include <M5Cardputer.h>

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.drawString("MODSYNTH", 10, 10);
}

void loop() {
    M5Cardputer.update();
}
