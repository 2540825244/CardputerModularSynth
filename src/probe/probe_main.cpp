// Hardware probe for the Cardputer-Adv (Task 2). Not part of the synth.
// Screen sections: BOARD / AUDIO / KEYS / HPDET / RAM / SD.
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

static constexpr i2s_port_t kI2sPort = I2S_NUM_1;
static constexpr int kRate = 48000;
static constexpr int kFrames = 64;

static volatile bool g_saw = false;      // false = 440 Hz sine, true = 100 Hz saw
static volatile bool g_audioStarted = false;
static volatile uint32_t g_audioBlocks = 0;
static volatile int g_i2sErr = 0;        // esp_err_t of install / set_pin / write
static int g_codecFails = 0;

// ---- audio: its own core-1 task, UI never touches I2S ----
static void audioTask(void*) {
    int16_t buf[kFrames * 2];
    float phSine = 0.f, phSaw = 0.f;
    const float amp = 16384.f;  // -6 dBFS
    for (;;) {
        const bool saw = g_saw;
        for (int i = 0; i < kFrames; i++) {
            float v;
            if (saw) {
                v = 2.f * phSaw - 1.f;
                phSaw += 100.f / kRate;
                if (phSaw >= 1.f) phSaw -= 1.f;
            } else {
                v = sinf(6.2831853f * phSine);
                phSine += 440.f / kRate;
                if (phSine >= 1.f) phSine -= 1.f;
            }
            int16_t s = (int16_t)(v * amp);
            buf[2 * i] = s;
            buf[2 * i + 1] = s;
        }
        size_t written = 0;
        esp_err_t e = i2s_write(kI2sPort, buf, sizeof(buf), &written, portMAX_DELAY);
        if (e != ESP_OK) {
            if (!g_i2sErr) g_i2sErr = e;
            vTaskDelay(1);  // never spin
        }
        g_audioBlocks = g_audioBlocks + 1;
    }
}

// Returns true when the driver is installed and pinned; only then is the audio task created.
static bool startAudio() {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = kRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = 64;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    esp_err_t e = i2s_driver_install(kI2sPort, &cfg, 0, nullptr);
    if (e != ESP_OK) {
        g_i2sErr = e;
        return false;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = 41;
    pins.ws_io_num = 43;
    pins.data_out_num = 42;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    e = i2s_set_pin(kI2sPort, &pins);
    if (e != ESP_OK) {
        g_i2sErr = e;
        return false;
    }
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 1);
    g_audioStarted = true;
    return true;
}

// Called from the core-0 UI task (I2C is core 0 only).
static void startCodec() {
    delay(50);  // let BCLK run before the codec (no MCLK) is configured
    // ES8311 sequence from M5Unified 0.2.28 _speaker_enabled_cb_cardputer_adv
    static const uint8_t regs[][2] = {{0x00, 0x80}, {0x01, 0xB5}, {0x02, 0x18}, {0x0D, 0x01},
                                      {0x12, 0x00}, {0x13, 0x10}, {0x32, 0xBF}, {0x37, 0x08}};
    for (auto& r : regs) {
        if (!M5.In_I2C.writeRegister8(0x18, r[0], r[1], 400000)) g_codecFails++;
    }
}

// ---- keys ----
struct KeyPos { int8_t x, y; };
static KeyPos g_prev[16];
static int g_prevN = 0;
static char g_hist[8][8];  // newest at [0]
static int g_histN = 0;

static void pushHist(char sign, int x, int y) {
    memmove(g_hist[1], g_hist[0], sizeof(g_hist[0]) * 7);
    snprintf(g_hist[0], sizeof(g_hist[0]), "%c%d,%d", sign, x, y);
    if (g_histN < 8) g_histN++;
}

static void pollKeys(KeyPos* cur, int& n) {
    n = 0;
    // The TCA8418 reader handles one event per update(): drain in a loop.
    for (int k = 0; k < 8; k++) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed() &&
            M5Cardputer.Keyboard.keysState().space) {
            g_saw = !g_saw;
        }
    }
    for (auto& p : M5Cardputer.Keyboard.keyList()) {
        if (n < 16) cur[n++] = {(int8_t)p.x, (int8_t)p.y};
    }
    for (int i = 0; i < n; i++) {
        bool was = false;
        for (int j = 0; j < g_prevN; j++) was |= (g_prev[j].x == cur[i].x && g_prev[j].y == cur[i].y);
        if (!was) pushHist('+', cur[i].x, cur[i].y);
    }
    for (int j = 0; j < g_prevN; j++) {
        bool is = false;
        for (int i = 0; i < n; i++) is |= (g_prev[j].x == cur[i].x && g_prev[j].y == cur[i].y);
        if (!is) pushHist('-', g_prev[j].x, g_prev[j].y);
    }
    memcpy(g_prev, cur, sizeof(KeyPos) * n);
    g_prevN = n;
}

// ---- HP_DET scan ----
static const int kCand[] = {1, 2, 3, 4, 5, 6, 7, 13, 15, 16, 17, 18, 45, 47, 48};
static constexpr int kNCand = sizeof(kCand) / sizeof(kCand[0]);
static uint8_t g_level[kNCand];
static uint16_t g_flips[kNCand];

static void scanInit() {
    for (int i = 0; i < kNCand; i++) {
        gpio_reset_pin((gpio_num_t)kCand[i]);
        gpio_set_direction((gpio_num_t)kCand[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)kCand[i], GPIO_FLOATING);
        g_level[i] = gpio_get_level((gpio_num_t)kCand[i]);
    }
}

static void scanPins() {
    for (int i = 0; i < kNCand; i++) {
        uint8_t l = gpio_get_level((gpio_num_t)kCand[i]);
        if (l != g_level[i]) {
            g_level[i] = l;
            if (g_flips[i] < 999) g_flips[i]++;
        }
    }
}

// ---- screen ----
static M5Canvas canvas(&M5Cardputer.Display);
static const uint16_t kBg = lgfx::color565(0xc4, 0xc6, 0xb8);
static constexpr uint16_t kFg = 0x18E3;   // approx #1d1f1a
static constexpr uint16_t kAcc = 0xFA63;  // approx #ff4d1a
static int g_row = 0;

static void head(const char* name) {
    canvas.setTextColor(kAcc, kBg);
    canvas.setCursor(2, g_row * 8 + 1);
    canvas.print(name);
    canvas.setTextColor(kFg, kBg);
    g_row++;
}

static void line(const char* fmt, ...) {
    char b[48];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    canvas.setCursor(2, g_row * 8 + 1);
    canvas.print(b);
    g_row++;
}

static uint32_t g_freeHeap, g_largest, g_minFree;        // before the sprite
static uint32_t g_freeHeap2, g_largest2, g_minFree2;     // after the sprite
static bool g_spriteOk = false;
static bool g_sdOk = false;
static uint8_t g_sdType = 0;
static uint64_t g_sdMb = 0;

static void draw(const KeyPos* cur, int n) {
    canvas.fillSprite(kBg);
    canvas.setTextSize(1);
    canvas.setFont(&fonts::Font0);
    g_row = 0;

    char b[48];
    line("id=%d %s", (int)M5.getBoard(), M5.getBoard() == m5::board_t::board_M5CardputerADV ? "CardputerADV" : "NOT ADV");

    head("AUDIO space=toggle");
    line("%s i2s=%d codec_fail=%d blk=%u", g_saw ? "SAW100" : "SINE440", (int)g_i2sErr, g_codecFails, (unsigned)g_audioBlocks);

    head("KEYS");
    int len = 0;
    b[0] = 0;
    for (int i = 0; i < n && len < 40; i++) len += snprintf(b + len, sizeof(b) - len, "(%d,%d)", cur[i].x, cur[i].y);
    line("%s", n ? b : "-");
    for (int r = 0; r < 2; r++) {
        len = 0;
        b[0] = 0;
        for (int i = r * 4; i < r * 4 + 4 && i < g_histN; i++) len += snprintf(b + len, sizeof(b) - len, "%-7s", g_hist[i]);
        line("%s", b[0] ? b : "");
    }

    head("HPDET (flips per pin)");
    len = 0;
    b[0] = 0;
    bool any = false;
    for (int i = 0; i < kNCand; i++) {
        if (g_flips[i]) {
            any = true;
            len += snprintf(b + len, sizeof(b) - len, "G%d:%u ", kCand[i], g_flips[i]);
            if (len > 36) break;
        }
    }
    line("%s", any ? b : "none yet (plug in/out)");
    len = 0;
    for (int i = 0; i < kNCand; i++) b[len++] = g_level[i] ? '1' : '0';
    b[len] = 0;
    line("lvl %s", b);

    head("RAM");
    line("pre  f=%u b=%u m=%u", (unsigned)g_freeHeap, (unsigned)g_largest, (unsigned)g_minFree);
    line("post f=%u b=%u m=%u", (unsigned)g_freeHeap2, (unsigned)g_largest2, (unsigned)g_minFree2);

    head("SD");
    if (g_sdOk) line("OK type=%d %uMB", g_sdType, (unsigned)g_sdMb);
    else line("FAIL (no card?)");

    canvas.pushSprite(0, 0);
}

// Keyboard, codec I2C and display all live on core 0; core 1 only runs audio.
static void uiTask(void*) {
    // "post" RAM: sprite and this task's stack are both allocated by now
    g_freeHeap2 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_largest2 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    g_minFree2 = ESP.getMinFreeHeap();
    if (g_audioStarted) startCodec();
    uint32_t lastScan = 0, lastDraw = 0;
    KeyPos cur[16];
    int n = 0;
    for (;;) {
        pollKeys(cur, n);
        uint32_t now = millis();
        if (now - lastScan >= 50) {
            lastScan = now;
            scanPins();
        }
        if (now - lastDraw >= 50) {
            lastDraw = now;
            draw(cur, n);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false;  // we own the codec
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(kBg);

    startAudio();
    scanInit();

    pinMode(5, INPUT_PULLUP);  // LoRa cap NSS (same bus): keep the radio off MISO
    SPI.begin(40, 39, 14, 12);
    g_sdOk = SD.begin(12, SPI, 25000000);
    if (g_sdOk) {
        g_sdType = SD.cardType();
        g_sdMb = SD.cardSize() / (1024 * 1024);
    }

    // RAM after init, before the canvas is allocated
    g_freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    g_minFree = ESP.getMinFreeHeap();

    canvas.setColorDepth(16);
    g_spriteOk = canvas.createSprite(240, 135) != nullptr;

    if (!g_spriteOk) {
        M5Cardputer.Display.setTextColor(kAcc, kBg);
        M5Cardputer.Display.drawString("SPRITE FAIL", 2, 2);
        vTaskDelete(nullptr);
        return;
    }
    xTaskCreatePinnedToCore(uiTask, "ui", 8192, nullptr, 1, nullptr, 0);
    vTaskDelete(nullptr);  // loop() never runs
}

void loop() {}
