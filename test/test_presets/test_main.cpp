#include <unity.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include "core/engine.h"
#include "core/modules/kit.h"
#include "core/modules/out.h"
#include "core/patch_io.h"
#include "core/presets.h"
#include "core/registry.h"
#include "core/dsp_util.h"

// The v1.3 preset bench (native stand-in for the device bench while the device is away): every
// preset loads, plays on its own for 20 s (slow LFOs, 20 bpm clocks and feedback build up; a KIT
// gets one PLAY and plays its pass), fits the CPU budget from the per-type costs (device-measured),
// and peaks near -3 dBFS at OUT's line volume before its soft clip (so nothing is squashed).

void setUp() {}
void tearDown() {}

static uint32_t bigHeap() { return 1000000; }

// OUT soft-clips: the level going in, from the level coming out (monotonic below 1).
static double unclip(double y) {
    if (y >= 1.0) return 3.0;
    double lo = 0, hi = 3;
    for (int i = 0; i < 40; i++) {
        const double mid = 0.5 * (lo + hi);
        (softClip((float)mid) < y ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

struct Level {
    double peak, rms;
    double step[12];  // a KIT preset: each step's peak (its PO-33 slice)
    int steps;        // 0 = no KIT
};

static Level play(const Patch& p, Engine& eng, float sec) {
    Level l{};
    int gap = 0;
    float buf[kBlock];
    // OUT's volume slews up from its start value: settle first, as on the device before PLAY
    bool kit = false;
    for (int i = 0; i < p.moduleCount(); i++) kit = kit || p.moduleAt(i).type == &kKitType;
    for (int b = 0; kit && b < (int)(0.5f * kSampleRate / kBlock); b++) {
        eng.processBlock(buf);
        eng.collect();
    }
    for (int i = 0; i < p.moduleCount(); i++)
        if (p.moduleAt(i).type == &kKitType) {
            static_cast<KitModule*>(eng.instance(p.moduleAt(i).id))->start();
            l.steps = (int)p.moduleAt(i).params[kit::kLen];
            gap = (int)(p.moduleAt(i).params[kit::kGap] * kSampleRate / 1000.f);
        }
    double sum = 0;
    // a KIT: its pass (the take), else sec
    int blocks = l.steps ? l.steps * gap / kBlock + (int)(0.3f * kSampleRate / kBlock) : (int)(sec * kSampleRate / kBlock);
    for (int b = 0; b < blocks; b++) {
        eng.processBlock(buf);
        eng.collect();
        for (int i = 0; i < kBlock; i++) {
            const double a = fabs((double)buf[i]);
            l.peak = a > l.peak ? a : l.peak;
            sum += (double)buf[i] * buf[i];
            const int s = gap ? (b * kBlock + i) / gap : 0;
            if (l.steps && s < l.steps && a > l.step[s]) l.step[s] = a;
        }
    }
    l.rms = sqrt(sum / (blocks * kBlock));
    l.peak = unclip(l.peak);
    for (int s = 0; s < l.steps; s++) l.step[s] = unclip(l.step[s]);
    return l;
}

static double db(double x) { return 20.0 * log10(x > 1e-9 ? x : 1e-9); }



void test_preset_bench() {
    int n = 0;
    const Preset* ps = presets(&n);
    printf("\n%-7s %5s %7s %7s  (peak before OUT's soft clip)\n", "PRESET", "CPU%", "PEAKdB", "RMSdB");
    int bad = 0;
    for (int i = 0; i < n; i++) {
        std::unique_ptr<Patch> p(new Patch());
        TEST_ASSERT_EQUAL_MESSAGE((int)LoadError::None, (int)patchFromJson(ps[i].json, *p, nullptr).error, ps[i].name);
        Engine eng(bigHeap);
        TEST_ASSERT_EQUAL_MESSAGE(0, eng.apply(*p), ps[i].name);
        const uint32_t cyc = eng.estimatedCycles();
        const Level l = play(*p, eng, 20.f);
        const bool cpuOk = cyc <= kCpuBudget, loud = db(l.peak) > -1.0, quiet = db(l.peak) < -9.0,
                   silent = db(l.rms) < -40.0;
        bool slice = false;  // a KIT step too quiet to be a usable PO-33 slice
        for (int s = 0; s < l.steps; s++) slice = slice || db(l.step[s]) < -18.0;
        printf("%-7s %5.1f %7.1f %7.1f %s%s%s%s%s", ps[i].name, 100.0 * cyc / kCyclesPerSample, db(l.peak), db(l.rms),
               cpuOk ? "" : " CPU", loud ? " CLIP" : "", quiet ? " QUIET" : "", silent ? " SILENT" : "",
               slice ? " SLICE" : "");
        if (l.steps) {
            printf("  steps:");
            for (int s = 0; s < l.steps; s++) printf(" %.0f", db(l.step[s]));
        }
        printf("\n");
        bad += !cpuOk || loud || quiet || silent || slice;
    }
    TEST_ASSERT_EQUAL_MESSAGE(0, bad, "presets flagged above");
}

void test_preset_library() {
    int n = 0;
    const Preset* ps = presets(&n);
    TEST_ASSERT_TRUE(n >= 30);
    TEST_ASSERT_TRUE(n <= 72);  // v1.4: two PRESETS pages of 36 cells
    for (int i = 0; i < n; i++) {
        std::unique_ptr<Patch> p(new Patch());
        patchFromJson(ps[i].json, *p, nullptr);
        int outs = 0;
        for (int k = 0; k < p->moduleCount(); k++)
            if (p->moduleAt(k).type == &kOutType) {
                outs++;
                TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.f, p->moduleAt(k).params[2], ps[i].name);  // dest LINE
            }
        TEST_ASSERT_EQUAL_MESSAGE(1, outs, ps[i].name);
        for (int k = i + 1; k < n; k++) TEST_ASSERT_TRUE_MESSAGE(strcmp(ps[i].name, ps[k].name) != 0, ps[i].name);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_preset_bench);
    RUN_TEST(test_preset_library);
    return UNITY_END();
}
