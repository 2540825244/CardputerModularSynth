// CPU benchmark (env:bench): where do the audio cycles go? Prints results over USB serial.
// Runs the DSP on core 1 like the audio task (no I2S) while core 0 is idle, redrawing at 30 fps
// like the UI, or redrawing flat out. Flash it into the modsyn slot; the results repeat.
#include <M5Cardputer.h>
#include <esp_attr.h>
#include <xtensa/hal.h>
#include "Filters/ladder.h"
#include "core/engine.h"
#include "core/modules/env.h"
#include "core/modules/grd.h"
#include "core/modules/keys.h"
#include "core/modules/osc.h"
#include "core/modules/out.h"
#include "core/modules/vca.h"
#include "core/modules/vcf.h"
#include "core/dsp_util.h"
#include "core/ladder.h"
#include "core/registry.h"
#include "render/renderer.h"

namespace {

struct NoStorage : Storage {
    bool available() override { return false; }
    bool read(const char*, std::string&) override { return false; }
    bool write(const char*, const std::string&) override { return false; }
    bool exists(const char*) override { return false; }
};

Patch gPatch;
Engine gEngine([]() -> uint32_t { return heap_caps_get_free_size(MALLOC_CAP_8BIT); });
NoStorage gStore;
UiState gState{};
volatile int gUiMode = 0;  // 0 idle, 1 30 fps, 2 flat out (1 ms between frames)
volatile uint32_t gFrames = 0;

uint32_t ccount() { return xthal_get_ccount(); }

void uiTask(void*) {
    uint32_t last = 0;
    for (;;) {
        const uint32_t now = millis();
        if (gUiMode == 2 || (gUiMode == 1 && now - last >= 33)) {
            last = now;
            renderFrame(gState, gPatch, gEngine, gStore, now, false);
            gFrames++;
        }
        vTaskDelay(1);  // core 0's idle task must run (task watchdog)
    }
}

uint16_t add(Patch& p, const ModuleTypeInfo& t, int cell) {
    uint16_t id = 0;
    p.addModule(t, 0, (uint8_t)cell, &id);
    return id;
}

// Runs `blocks` blocks; returns cycles per sample (whole engine).
float runBlocks(int blocks) {
    static float mono[kBlock];
    uint64_t sum = 0;
    for (int b = 0; b < blocks; b++) {
        const uint32_t t0 = ccount();
        gEngine.processBlock(mono);
        sum += ccount() - t0;
        if ((b & 15) == 15) gEngine.collect();
    }
    return (float)sum / ((float)blocks * kBlock);
}

const char* kModes[] = {"idle", "ui30", "uiMax"};

// Patches are 5.3 KB: statics, never on the task stack.
Patch gBuild, gEmpty;

void voicePatch() {
    Patch& p = gBuild;
    p = gEmpty;
    const uint16_t k = add(p, kKeysType, 0), o = add(p, kOscType, 1), f = add(p, kVcfType, 2),
                   a = add(p, kVcaType, 3), e = add(p, kEnvType, 4), out = add(p, kOutType, 5);
    p.addCable(k, 0, o, 0);
    p.addCable(o, 0, f, 0);
    p.addCable(f, 0, a, 0);
    p.addCable(k, 1, e, 0);
    p.addCable(e, 0, a, 1);
    p.addCable(a, 0, out, 0);
    gPatch = p;
    gEngine.apply(gPatch);
    static_cast<KeysModule*>(gEngine.instance(k))->noteOn(60);
}

void printModules(const char* tag) {
    Serial.printf("  %s modules:", tag);
    for (int i = 0; i < gPatch.moduleCount(); i++) {
        const ModuleRec& r = gPatch.moduleAt(i);
        if (Module* m = gEngine.instance(r.id)) Serial.printf(" %s=%u", r.type->id, (unsigned)m->measuredCycles());
    }
    Serial.println();
}

// ---- micro benchmarks ----
volatile float gSink;

__attribute__((noinline)) float divLoop(float a, int n) {
    float s = 0.f;
    for (int i = 0; i < n; i++) s += 1.f / (a + (float)i);
    return s;
}
__attribute__((noinline)) float mulLoop(float a, int n) {
    float s = 0.f;
    for (int i = 0; i < n; i++) s += 0.999f * (a + (float)i);
    return s;
}

// The same VCA-like loop from flash and from IRAM.
__attribute__((noinline)) void vcaFlash(const float* in, float* out, float g, int n) {
    for (int r = 0; r < n; r++)
        for (int i = 0; i < kBlock; i++) out[i] = in[i] * g + out[i] * 0.001f;
}
__attribute__((noinline)) IRAM_ATTR void vcaIram(const float* in, float* out, float g, int n) {
    for (int r = 0; r < n; r++)
        for (int i = 0; i < kBlock; i++) out[i] = in[i] * g + out[i] * 0.001f;
}

void micro() {
    const int n = 20000;
    uint32_t t0 = ccount();
    gSink = divLoop(1.5f, n);
    const float div = (float)(ccount() - t0) / n;
    t0 = ccount();
    gSink = mulLoop(1.5f, n);
    const float mul = (float)(ccount() - t0) / n;
    Serial.printf("  float div loop %.1f cyc/iter, mul loop %.1f cyc/iter\n", div, mul);

    static daisysp::LadderFilter lad;
    lad.Init(48000.f);
    lad.SetFreq(1200.f);
    lad.SetRes(0.4f);
    float x = 0.f;
    t0 = ccount();
    for (int i = 0; i < n; i++) x = lad.Process(x * 0.5f + ((i & 64) ? 0.3f : -0.3f));
    gSink = x;
    Serial.printf("  daisysp LadderFilter::Process %.1f cyc/sample\n", (float)(ccount() - t0) / n);
    static Ladder fl;
    fl.Init(48000.f);
    fl.SetFreq(1200.f);
    fl.SetRes(0.4f);
    x = 0.f;
    t0 = ccount();
    for (int i = 0; i < n; i++) x = fl.Process(x * 0.5f + ((i & 64) ? 0.3f : -0.3f));
    gSink = x;
    Serial.printf("  fast Ladder::Process %.1f cyc/sample\n", (float)(ccount() - t0) / n);

    static float in[kBlock], out[kBlock];
    for (int i = 0; i < kBlock; i++) in[i] = (float)i * 0.01f;
    for (int pass = 0; pass < 2; pass++) {
        // cold: evict between calls by letting the UI run; reps=1 per call to mimic one block
        uint64_t f = 0, r = 0;
        for (int k = 0; k < 300; k++) {
            uint32_t a = ccount();
            vcaFlash(in, out, 0.7f, 1);
            f += ccount() - a;
            a = ccount();
            vcaIram(in, out, 0.7f, 1);
            r += ccount() - a;
            vTaskDelay(pass);  // pass 1: let core 0 run between calls (cache churn)
        }
        Serial.printf("  vca-like block %s: flash %.1f, IRAM %.1f cyc/sample\n", pass ? "after yield" : "back to back",
                      (float)f / (300.f * kBlock), (float)r / (300.f * kBlock));
    }
}

// ---- round 2: where a trivial module's ~180 cycles/sample go ----
__attribute__((noinline)) float recipNewton(float x) {
    float r;
    asm volatile("recip0.s %0, %1" : "=f"(r) : "f"(x));
    r = r * (2.f - x * r);
    r = r * (2.f - x * r);
    return r;
}
__attribute__((noinline)) float recipLoop(float a, int n) {
    float s = 0.f;
    for (int i = 0; i < n; i++) s += recipNewton(a + (float)i);
    return s;
}

void split() {
    const int n = 20000;
    uint32_t t0 = ccount();
    gSink = recipLoop(1.5f, n);
    Serial.printf("  recip0.s+2 Newton %.1f cyc/iter (div ~66); 1/3 -> %.7f\n", (float)(ccount() - t0) / n,
                  recipNewton(3.f));
    static float buf[kBlock];
    for (int i = 0; i < kBlock; i++) buf[i] = (float)i * 0.1f;
    t0 = ccount();
    for (int r = 0; r < 500; r++)
        for (int i = 0; i < kBlock; i++) buf[i] = sanitize(buf[i]);
    Serial.printf("  sanitize %.1f cyc/sample\n", (float)(ccount() - t0) / (500.f * kBlock));
    Slew sl;
    sl.setTimeMs(10.f, 48000.f);
    t0 = ccount();
    for (int r = 0; r < 500; r++)
        for (int i = 0; i < kBlock; i++) buf[i] = sl.process(buf[i]);
    Serial.printf("  Slew::process %.1f cyc/sample\n", (float)(ccount() - t0) / (500.f * kBlock));

    // one module's process() alone vs inside the engine
    Patch& p = gBuild;
    p = gEmpty;
    const uint16_t v = add(p, kVcaType, 0), o = add(p, kOscType, 1), k = add(p, kKeysType, 2), u = add(p, kOutType, 3);
    p.addCable(o, 0, v, 0);
    p.addCable(v, 0, u, 0);
    gPatch = p;
    gEngine.apply(gPatch);
    runBlocks(200);
    static float in0[kBlock], in1[kBlock];
    for (int i = 0; i < kBlock; i++) in0[i] = 1.f;
    const float* ins[kMaxIns] = {in0, in1};
    bool conn[kMaxIns] = {true, false};
    const struct { const char* n; uint16_t id; } mods[] = {{"vca", v}, {"osc", o}, {"keys", k}, {"out", u}};
    for (auto& m : mods) {
        Module* mm = gEngine.instance(m.id);
        t0 = ccount();
        for (int r = 0; r < 300; r++) mm->process(ins, conn);
        Serial.printf("  %s process() alone %.1f cyc/sample, in engine %u\n", m.n, (float)(ccount() - t0) / (300.f * kBlock),
                      (unsigned)mm->measuredCycles());
    }
    const float c = runBlocks(500);
    Serial.printf("  engine total for those 4: %.0f cyc/sample\n", c);
    // 16 unconnected VCAs: pure per-node overhead
    p = gEmpty;
    for (int i = 0; i < 16; i++) add(p, kVcaType, i);
    gPatch = p;
    gEngine.apply(gPatch);
    runBlocks(200);
    Serial.printf("  16 idle VCAs: %.0f cyc/sample total (%.0f each)\n", runBlocks(500), runBlocks(500) / 16.f);
    gEngine.apply(gEmpty);
    runBlocks(10);
    Serial.printf("  empty engine: %.1f cyc/sample\n", runBlocks(500));
}

// Every type alone, an OSC into each of its inputs, UI at 30 fps: its measured cycles/sample.
void allTypes() {
    int n = 0;
    const ModuleTypeInfo* const* types = ::allTypes(&n);
    Serial.println("types (cyc/sample in engine, ui30; est = table value):");
    gUiMode = 1;
    for (int t = 0; t < n; t++) {
        const ModuleTypeInfo& ty = *types[t];
        Patch& p = gBuild;
        p = gEmpty;
        const uint16_t o = add(p, kOscType, 0);
        uint16_t id = 0;
        if (&ty == &kOscType) id = o;
        else if (p.addModule(ty, 0, 1, &id) != PatchError::None) continue;
        if (id != o)
            for (int k = 0; k < ty.numIns; k++) p.addCable(o, 0, id, (uint8_t)k);
        gPatch = p;
        gEngine.apply(gPatch);
        if (!gEngine.instance(id)) {
            Serial.printf("  %s: not created\n", ty.id);
            continue;
        }
        runBlocks(200);
        runBlocks(300);
        Serial.printf("  %s=%u/%u\n", ty.id, (unsigned)gEngine.instance(id)->measuredCycles(), (unsigned)ty.cpuCycles);
        gEngine.apply(gEmpty);
        runBlocks(4);
    }
    gUiMode = 0;
}

void benchTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    gEngine.setCycleCounter(ccount);
    for (int round = 1;; round++) {
        Serial.printf("\n==== bench round %d (cpu %u MHz, budget 5000 cyc/sample) ====\n", round,
                      (unsigned)getCpuFrequencyMhz());
        Serial.println("split (ui idle):");
        gUiMode = 0;
        split();
        voicePatch();
        for (int mode = 0; mode < 3; mode++) {
            gUiMode = mode;
            vTaskDelay(pdMS_TO_TICKS(100));
            gFrames = 0;
            runBlocks(300);  // settle the per-module averages
            const uint32_t f0 = gFrames, t0 = millis();
            const float c = runBlocks(1500);
            const float fps = (gFrames - f0) * 1000.f / (float)(millis() - t0 + 1);
            Serial.printf("voice6 %-5s: %.0f cyc/sample = %.1f%% (ui %.0f fps)\n", kModes[mode], c, c / 50.f, fps);
            printModules(kModes[mode]);
        }
        gUiMode = 0;
        vTaskDelay(pdMS_TO_TICKS(50));
        Serial.println("micro (ui idle):");
        micro();
        allTypes();

        // GRD alone, every engine, with the UI at 30 fps (the real case) and idle.
        Patch& g = gBuild;
        g = gEmpty;
        const uint16_t id = add(g, kGrdType, 0);
        add(g, kOutType, 1);
        g.addCable(id, 0, g.moduleAt(1).id, 0);
        gPatch = g;
        gEngine.apply(gPatch);
        Serial.println("GRD per ENG value, ui idle (cyc/sample: ladder LP / filter OFF):");
        for (int e = 0; e < 24; e++) {
            gPatch.find(id)->params[0] = (float)e;
            gPatch.find(id)->params[12] = 0.f;  // FLT LP
            gEngine.apply(gPatch);
            gUiMode = 0;
            runBlocks(150);
            const float cl = runBlocks(400);
            gPatch.find(id)->params[12] = 2.f;  // FLT OFF
            gEngine.apply(gPatch);
            runBlocks(150);
            const float co = runBlocks(400);
            Serial.printf("  eng %2d: %5.0f / %5.0f\n", e, cl, co);
        }
        gUiMode = 0;
        gEngine.apply(gEmpty);
        Serial.printf("heap free=%u largest=%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    M5Cardputer.Display.setRotation(1);
    renderBegin();
    xTaskCreatePinnedToCore(uiTask, "ui", 12288, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(benchTask, "bench", 12288, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
    vTaskDelete(nullptr);
}

void loop() {}
