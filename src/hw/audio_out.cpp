#include "audio_out.h"
#include <M5Unified.h>
#include <driver/i2s.h>
#include <xtensa/hal.h>

namespace {

constexpr i2s_port_t kI2sPort = I2S_NUM_1;
constexpr int kRate = (int)kSampleRate;
constexpr float kBlockBudgetCycles = 240e6f * kBlock / kSampleRate;

volatile int g_i2sErr = 0;
int g_codecFails = 0;
Engine* g_engine = nullptr;
volatile uint32_t g_stackHw = 0;

// Own core-1 task; the UI never touches I2S.
void audioTask(void*) {
    static float mono[kBlock];
    static int16_t buf[kBlock * 2];
    float load = 0.f;
    uint32_t blocks = 0;
    for (;;) {
        const uint32_t t0 = xthal_get_ccount();
        g_engine->processBlock(mono);
        const uint32_t cycles = xthal_get_ccount() - t0;
        for (int i = 0; i < kBlock; i++) {
            float v = mono[i];
            v = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
            const int16_t s = (int16_t)(v * 32767.f);
            buf[2 * i] = s;
            buf[2 * i + 1] = s;
        }
        load += 0.02f * ((float)cycles / kBlockBudgetCycles - load);
        g_engine->setCpuLoad(load);
        if ((++blocks & 255) == 0) g_stackHw = uxTaskGetStackHighWaterMark(nullptr);
        size_t written = 0;
        const esp_err_t e = i2s_write(kI2sPort, buf, sizeof(buf), &written, portMAX_DELAY);
        if (e != ESP_OK) {
            if (!g_i2sErr) g_i2sErr = e;
            vTaskDelay(1);  // never spin
        }
    }
}

}  // namespace

bool audioBegin(Engine& engine) {
    g_engine = &engine;
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
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
    return true;
}

void codecBegin() {
    delay(50);  // let BCLK run before the codec (no MCLK) is configured
    // ES8311 sequence from M5Unified 0.2.28 _speaker_enabled_cb_cardputer_adv
    static const uint8_t regs[][2] = {{0x00, 0x80}, {0x01, 0xB5}, {0x02, 0x18}, {0x0D, 0x01},
                                      {0x12, 0x00}, {0x13, 0x10}, {0x32, 0xBF}, {0x37, 0x08}};
    for (auto& r : regs)
        if (!M5.In_I2C.writeRegister8(0x18, r[0], r[1], 400000)) g_codecFails++;
}

int audioI2sError() { return g_i2sErr; }
int codecFailures() { return g_codecFails; }
uint32_t audioStackHighWater() { return g_stackHw; }
