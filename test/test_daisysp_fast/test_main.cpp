// v1.3 Task 3: the control-rate / fast-math DaisySP edits must sound like the old code.
// Each test runs the real module (src/core/modules + the edited lib/DaisySP) and, next to it,
// the old module code on the unmodified DaisySP copy in ref/ (namespace daisysp_ref), from the
// same inputs, and compares the outputs: with static params, and with a param change mid-run
// (the steady state after it must match). Tolerances are relative to the reference's peak.
#include <unity.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "core/dsp_util.h"
#include "core/module.h"
#include "core/modules/common.h"
#include "core/modules/proc.h"
#include "core/modules/src.h"
#include "ref/analogbassdrum.h"
#include "ref/analogsnaredrum.h"
#include "ref/autowah.h"
#include "ref/formantosc.h"
#include "ref/hihat.h"
#include "ref/modalvoice.h"
#include "ref/resonator.h"
#include "ref/stringvoice.h"
#include "ref/vosim.h"
#include "ref/zoscillator.h"
#include "ref/svf.h"
#include "Filters/svf.h"

using namespace mods;

void setUp() {}
void tearDown() {}

namespace {

constexpr int kMaxIn = 4;

// Whole-run input signals, one vector per input (empty = unconnected).
struct Inputs {
    std::vector<float> ch[kMaxIn];
    const float* at(int k, int block) const { return ch[k].empty() ? nullptr : ch[k].data() + block * kBlock; }
};

// A param change at block `block` (block < 0: none).
struct Change {
    int block = -1, index = 0;
    float value = 0.f;
};

// Params seen by the old code: the type's defaults, overrides, then the change.
struct Params {
    float p[kMaxParams];
    Params(const ModuleTypeInfo& t, std::initializer_list<std::pair<int, float>> set) {
        for (int i = 0; i < kMaxParams; i++) p[i] = i < t.numParams ? t.params[i].def : 0.f;
        for (auto& s : set) p[s.first] = s.second;
    }
    float operator()(int i) const { return p[i]; }
};

std::vector<float> runNew(const ModuleTypeInfo& t, const Params& ps, const Inputs& in, int blocks, Change ch) {
    Module* m = t.create();
    for (int i = 0; i < t.numParams; i++) m->setParam(i, ps.p[i]);
    std::vector<float> r;
    for (int b = 0; b < blocks; b++) {
        if (b == ch.block) m->setParam(ch.index, ch.value);
        const float* ins[kMaxIns] = {};
        bool conn[kMaxIns] = {};
        for (int k = 0; k < kMaxIn; k++) {
            ins[k] = in.at(k, b);
            conn[k] = ins[k] != nullptr;
        }
        m->process(ins, conn);
        r.insert(r.end(), m->out(0), m->out(0) + kBlock);
    }
    delete m;
    return r;
}

// The old process() bodies, verbatim apart from param(k) -> p(k) and daisysp -> daisysp_ref.
struct OldTrig {
    GateDetector g;
    bool operator()(const float* in, int i) {
        if (!in) return false;
        g.process(in[i]);
        return g.rose;
    }
};
float oldBaseVolts(const Params& p, int oct) { return p(oct) + p(oct + 1) * (1.f / 12.f); }

template <typename Body>
std::vector<float> runOld(Params p, const Inputs& in, int blocks, Change ch, Body body) {
    std::vector<float> r;
    float o[kBlock];
    for (int b = 0; b < blocks; b++) {
        if (b == ch.block) p.p[ch.index] = ch.value;
        const float* ins[kMaxIn];
        for (int k = 0; k < kMaxIn; k++) ins[k] = in.at(k, b);
        body(p, ins, o);
        r.insert(r.end(), o, o + kBlock);
    }
    return r;
}

// Worst |new - old| over [from, end), relative to the old peak over the whole run.
float relErr(const std::vector<float>& a, const std::vector<float>& b, size_t from = 0) {
    float peak = 1e-3f, worst = 0.f;
    for (float v : b) peak = fmaxf(peak, fabsf(v));
    for (size_t i = from; i < a.size(); i++) worst = fmaxf(worst, fabsf(a[i] - b[i]));
    return worst / peak;
}

void expectClose(const char* what, const std::vector<float>& a, const std::vector<float>& b, float tol,
                 size_t from = 0) {
    float peak = 0.f;
    for (float v : b) peak = fmaxf(peak, fabsf(v));
    const float e = relErr(a, b, from);
    char m[160];
    snprintf(m, sizeof m, "%s: rel err %.3g (tol %.3g, peak %.3g)", what, (double)e, (double)tol, (double)peak);
    if (!(peak > 0.05f)) TEST_FAIL_MESSAGE((std::string(m) + ": silent reference").c_str());
    if (!(e <= tol)) TEST_FAIL_MESSAGE(m);
    TEST_MESSAGE(m);
}

// Gate pulses: 2 ms high every `period` samples, starting at `first`.
std::vector<float> triggers(int blocks, int period, int first = 10) {
    std::vector<float> v(blocks * kBlock, 0.f);
    for (int i = first; i < (int)v.size(); i += period)
        for (int k = 0; k < 96 && i + k < (int)v.size(); k++) v[i + k] = 5.f;
    return v;
}
std::vector<float> constant(int blocks, float x) { return std::vector<float>(blocks * kBlock, x); }
std::vector<float> ramp(int blocks, float a, float b) {
    std::vector<float> v(blocks * kBlock);
    for (size_t i = 0; i < v.size(); i++) v[i] = a + (b - a) * (float)i / (float)v.size();
    return v;
}
std::vector<float> noiseIn(int blocks, float amp) {
    std::vector<float> v(blocks * kBlock);
    uint32_t s = 7;
    for (size_t i = 0; i < v.size(); i++) {
        s = s * 1664525u + 1013904223u;
        const float w = ((float)(s >> 8) / 16777216.f - 0.5f) * 2.f;
        v[i] = amp * w * (((i / 2400) & 1) ? 1.f : 0.2f);  // loud and quiet stretches
    }
    return v;
}

// Runs new and old twice: static params, then a change at mid-run; srand() before each run
// because the DaisySP drums and strings draw noise from rand().
// `make` returns a fresh old-code body (its own DaisySP objects) for each run.
template <typename Make>
void compare(const char* name, const ModuleTypeInfo& t, std::initializer_list<std::pair<int, float>> set,
             const Inputs& in, int blocks, Change ch, float tol, Make make) {
    const Params ps(t, set);
    srand(1);
    const auto a = runNew(t, ps, in, blocks, {});
    srand(1);
    const auto b = runOld(ps, in, blocks, {}, make());
    expectClose((std::string(name) + " static").c_str(), a, b, tol);
    srand(1);
    const auto c = runNew(t, ps, in, blocks, ch);
    srand(1);
    const auto d = runOld(ps, in, blocks, ch, make());
    // the steady state after the change: one block of slack for a control-rate update
    expectClose((std::string(name) + " after change").c_str(), c, d, tol, (size_t)(ch.block + 1) * kBlock);
}

}  // namespace

// ---------------------------------------------------------------- Svf
// The repeat cache in SetFreq/SetRes must be exact for any call order (VCF, drums, strings).
void test_svf_cache_is_exact() {
    daisysp::Svf a;
    daisysp_ref::Svf b;
    a.Init(kSampleRate);
    b.Init(kSampleRate);
    // stable settings only (res 1 with a high cutoff and drive runs away to NaN in both)
    const float fs[] = {80.f, 800.f, 800.f, 5000.f, 12000.f};
    const float rs[] = {0.f, 0.3f, 0.3f, 0.8f, 0.9f};
    const float ds[] = {0.f, 0.5f, 1.f};
    uint32_t seed = 3;
    auto rnd = [&seed](int n) {
        seed = seed * 1664525u + 1013904223u;
        return (int)((seed >> 16) % (uint32_t)n);
    };
    for (int i = 0; i < 20000; i++) {
        switch (rnd(8)) {  // a random mix of calls with repeated values, in any order
            case 0: {
                const float f = fs[rnd(5)];
                a.SetFreq(f);
                b.SetFreq(f);
            } break;
            case 1: {
                const float r = rs[rnd(5)];
                a.SetRes(r);
                b.SetRes(r);
            } break;
            case 2: {
                const float d = ds[rnd(3)];
                a.SetDrive(d);
                b.SetDrive(d);
            } break;
            default: break;
        }
        const float x = ((float)rnd(2001) / 1000.f - 1.f) * 0.1f;
        a.Process(x);
        b.Process(x);
        if (!(fabsf(b.Band()) < 1e6f)) TEST_FAIL_MESSAGE("reference Svf ran away: the test needs stable settings");
        if (a.Low() != b.Low() || a.High() != b.High() || a.Band() != b.Band() || a.Notch() != b.Notch() ||
            a.Peak() != b.Peak()) {
            char m[160];
            snprintf(m, sizeof m, "Svf differs at sample %d: band %g vs %g low %g vs %g", i, (double)a.Band(),
                     (double)b.Band(), (double)a.Low(), (double)b.Low());
            TEST_FAIL_MESSAGE(m);
        }
    }
}

// NaN and signed zeros through the cache: NaN never equals the last value (always recomputed,
// like the original); -0 equals +0 (skipped, and the original gives the same coefficients).
void test_svf_cache_nan_and_zero() {
    daisysp::Svf a;
    daisysp_ref::Svf b;
    a.Init(kSampleRate);
    b.Init(kSampleRate);
    const float nan = __builtin_nanf("");
    auto both = [&](auto f) {
        f(a);
        f(b);
    };
    both([&](auto& v) { v.SetFreq(nan); });
    both([&](auto& v) { v.SetFreq(nan); });
    both([&](auto& v) { v.SetRes(nan); });
    both([&](auto& v) { v.SetFreq(1000.f); });
    both([&](auto& v) { v.SetRes(0.f); });
    both([&](auto& v) { v.SetRes(-0.f); });
    both([&](auto& v) { v.SetFreq(0.f); });
    both([&](auto& v) { v.SetFreq(-0.f); });
    both([&](auto& v) { v.SetFreq(1000.f); });
    both([&](auto& v) { v.SetRes(0.5f); });
    for (int i = 0; i < 2000; i++) {
        const float x = (i & 32) ? 0.1f : -0.1f;
        a.Process(x);
        b.Process(x);
        TEST_ASSERT_TRUE(fabsf(b.Band()) < 1e6f);
        TEST_ASSERT_TRUE(a.Band() == b.Band() && a.Low() == b.Low() && a.High() == b.High());
    }
}

// ---------------------------------------------------------------- drums
// Old wrapper state: the DaisySP object plus the trigger detector.
template <typename D>
struct OldDrum {
    D d;
    OldTrig trig;
    OldDrum() { d.Init(kSampleRate); }
};

void test_snr_matches_old() {
    const int n = 1500;  // 1 s, a hit every 187 ms
    Inputs in;
    in.ch[0] = triggers(n, 9000);
    in.ch[1] = ramp(n, -2.f, 2.f);  // ACC input: each hit's accent differs
    auto make = [] {
        auto s = std::make_shared<OldDrum<daisysp_ref::AnalogSnareDrum>>();
        return [s](const Params& p, const float* const* ins, float* o) {
            enum { kFreq, kTone, kDecay, kSnappy, kAccent };
            const float *tr = ins[0], *acc = ins[1], *pit = ins[2];
            s->d.SetTone(p(kTone));
            s->d.SetDecay(p(kDecay));
            s->d.SetSnappy(p(kSnappy));
            for (int i = 0; i < kBlock; i++) {
                const bool t = s->trig(tr, i);
                if (t) {
                    s->d.SetFreq(clampf(p(kFreq) * fastExp2(at(pit, i)), 50.f, 700.f));
                    s->d.SetAccent(clampf(p(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
                }
                o[i] = clampf(s->d.Process(t) * 0.7f, -2.f, 2.f) * kAudioVolts;
            }
        };
    };
    compare("snr 808 modes", kSnrType, {{1, 0.3f}}, in, n, {700, 2, 0.8f}, 1e-4f, make);
    compare("snr extra modes", kSnrType, {{1, 0.9f}}, in, n, {700, 3, 0.2f}, 1e-4f, make);
    compare("snr FRQ change", kSnrType, {}, in, n, {700, 0, 320.f}, 1e-4f, make);
}

void test_kck_matches_old() {
    const int n = 1500;
    Inputs in;
    in.ch[0] = triggers(n, 12000);
    in.ch[1] = ramp(n, 2.f, -2.f);  // ACC input
    in.ch[2] = constant(n, 0.5f);  // half an octave up
    auto make = [] {
        auto s = std::make_shared<OldDrum<daisysp_ref::AnalogBassDrum>>();
        return [s](const Params& p, const float* const* ins, float* o) {
            enum { kFreq, kTone, kDecay, kAtkFm, kSelfFm, kAccent };
            const float *tr = ins[0], *acc = ins[1], *pit = ins[2];
            s->d.SetTone(p(kTone));
            s->d.SetDecay(p(kDecay));
            s->d.SetAttackFmAmount(p(kAtkFm));
            s->d.SetSelfFmAmount(p(kSelfFm));
            for (int i = 0; i < kBlock; i++) {
                const bool t = s->trig(tr, i);
                if (t) {
                    s->d.SetFreq(clampf(p(kFreq) * fastExp2(at(pit, i)), 20.f, 400.f));
                    s->d.SetAccent(clampf(p(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
                }
                o[i] = clampf(s->d.Process(t) * 3.4f, -2.f, 2.f) * kAudioVolts;
            }
        };
    };
    // the resonator retunes every sample (self FM) through the fast SetFreq/SetRes: a few ulps
    // per sample, hence the looser tolerance
    compare("kck", kKckType, {{4, 0.6f}}, in, n, {700, 1, 0.9f}, 1e-3f, make);
    compare("kck FRQ change", kKckType, {}, in, n, {700, 0, 90.f}, 1e-3f, make);
}

void test_hat_matches_old() {
    const int n = 1200;
    Inputs in;
    in.ch[0] = triggers(n, 6000);
    in.ch[1] = ramp(n, -2.f, 2.f);  // ACC input
    auto make = [] {
        auto s = std::make_shared<OldDrum<daisysp_ref::HiHat<>>>();
        return [s](const Params& p, const float* const* ins, float* o) {
            enum { kFreq, kTone, kDecay, kNoise, kAccent };
            const float *tr = ins[0], *acc = ins[1], *pit = ins[2];
            s->d.SetTone(p(kTone));
            s->d.SetDecay(p(kDecay));
            s->d.SetNoisiness(p(kNoise));
            for (int i = 0; i < kBlock; i++) {
                const bool t = s->trig(tr, i);
                if (t) {
                    s->d.SetFreq(clampf(p(kFreq) * fastExp2(at(pit, i)), 500.f, 16000.f));
                    s->d.SetAccent(clampf(p(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
                }
                o[i] = clampf(s->d.Process(t) * 4.5f, -2.f, 2.f) * kAudioVolts;
            }
        };
    };
    compare("hat", kHatType, {}, in, n, {600, 1, 0.2f}, 1e-4f, make);
    compare("hat FRQ change", kHatType, {}, in, n, {600, 0, 7000.f}, 1e-4f, make);
}

// ---------------------------------------------------------------- physical models
template <typename RefVoice>
void physCompare(const char* name, const ModuleTypeInfo& t, float gain, bool pitchRamp) {
    const int n = 1500;
    Inputs in;
    in.ch[0] = triggers(n, 9000);
    in.ch[1] = pitchRamp ? ramp(n, -0.5f, 0.5f) : constant(n, 0.f);
    in.ch[2] = ramp(n, -2.f, 2.f);  // ACC input
    auto make = [gain] {
        auto s = std::make_shared<OldDrum<RefVoice>>();
        return [s, gain](const Params& p, const float* const* ins, float* o) {
            enum { kOct, kTune, kStruct, kBright, kDamp, kAccent };
            const float *tr = ins[0], *pit = ins[1], *acc = ins[2], *br = ins[3];
            const float base = oldBaseVolts(p, kOct);
            s->d.SetStructure(p(kStruct));
            s->d.SetDamping(p(kDamp));
            for (int i = 0; i < kBlock; i++) {
                const bool t = s->trig(tr, i);
                if (t || i == 0) {
                    s->d.SetFreq(hzClamp(voltsToHz(base + at(pit, i)), 20.f, 8000.f));
                    s->d.SetBrightness(clampf(p(kBright) + at(br, i) * 0.2f, 0.f, 1.f));
                    s->d.SetAccent(clampf(p(kAccent) + at(acc, i) * 0.2f, 0.f, 1.f));
                }
                o[i] = clampf(s->d.Process(t) * gain, -2.f, 2.f) * kAudioVolts;
            }
        };
    };
    compare(name, t, {}, in, n, {700, 4, 0.8f}, 1e-4f, make);                                // DAMP
    compare((std::string(name) + " structure").c_str(), t, {{2, 0.8f}}, in, n, {700, 3, 0.2f}, 1e-4f, make);  // BRI
}

void test_mdl_matches_old() { physCompare<daisysp_ref::ModalVoice>("mdl", kMdlType, 0.5f, true); }
void test_str_matches_old() { physCompare<daisysp_ref::StringVoice>("str", kStrType, 1.2f, false); }

void test_res_matches_old() {
    const int n = 1200;
    Inputs in;
    in.ch[0] = noiseIn(n, 0.5f);
    in.ch[1] = ramp(n, -1.f, 1.f);
    auto make = [] {
        auto r = std::make_shared<daisysp_ref::Resonator>();
        r->Init(0.015f, 16, kSampleRate);
        return [r](const Params& p, const float* const* ins, float* o) {
            enum { kOct, kTune, kStruct, kBright, kDamp };
            const float *in = ins[0], *pit = ins[1];
            r->SetFreq(hzClamp(voltsToHz(oldBaseVolts(p, kOct) + at(pit, 0)), 20.f, 8000.f));
            r->SetStructure(p(kStruct));
            r->SetBrightness(p(kBright));
            r->SetDamping(p(kDamp));
            for (int i = 0; i < kBlock; i++) o[i] = clampf(r->Process(in ? in[i] * 0.2f : 0.f), -2.f, 2.f) * kAudioVolts;
        };
    };
    compare("res", kResType, {}, in, n, {600, 4, 0.9f}, 1e-4f, make);
}

// ---------------------------------------------------------------- oscillators and AWH
// sinf -> FastSin2Pi and freq / sr -> freq * (1 / sr): a few ulps per sample
void test_vos_matches_old() {
    const int n = 600;
    Inputs in;
    in.ch[0] = ramp(n, -1.f, 1.f);
    in.ch[1] = ramp(n, 0.5f, -0.5f);
    auto make = [] {
        auto v = std::make_shared<daisysp_ref::VosimOscillator>();
        v->Init(kSampleRate);
        return [v](const Params& p, const float* const* ins, float* o) {
            enum { kOct, kTune, kForm1, kForm2, kShape };
            const float *pit = ins[0], *fr = ins[1];
            const float base = oldBaseVolts(p, kOct);
            v->SetShape(p(kShape) * 2.f - 1.f);
            for (int i = 0; i < kBlock; i++) {
                const float f = fastExp2(at(fr, i));
                v->SetFreq(hzClamp(voltsToHz(base + at(pit, i))));
                v->SetForm1Freq(hzClamp(p(kForm1) * f));
                v->SetForm2Freq(hzClamp(p(kForm2) * f));
                o[i] = clampf(v->Process(), -1.5f, 1.5f) * kAudioVolts;
            }
        };
    };
    compare("vos", kVosType, {}, in, n, {300, 4, 0.9f}, 1e-3f, make);
}

void test_zos_matches_old() {
    const int n = 600;
    Inputs in;
    in.ch[0] = ramp(n, -1.f, 1.f);
    in.ch[1] = ramp(n, -2.f, 2.f);
    auto make = [] {
        auto z = std::make_shared<daisysp_ref::ZOscillator>();
        z->Init(kSampleRate);
        return [z](const Params& p, const float* const* ins, float* o) {
            enum { kOct, kTune, kFormant, kShape, kMode };
            const float *pit = ins[0], *sh = ins[1];
            const float base = oldBaseVolts(p, kOct);
            z->SetFormantFreq(p(kFormant));
            z->SetMode(p(kMode) * 2.f - 1.f);
            for (int i = 0; i < kBlock; i++) {
                z->SetFreq(hzClamp(voltsToHz(base + at(pit, i))));
                z->SetShape(clampf(p(kShape) + at(sh, i) * 0.2f, 0.f, 1.f));
                o[i] = clampf(z->Process(), -1.5f, 1.5f) * kAudioVolts;
            }
        };
    };
    compare("zos", kZosType, {}, in, n, {300, 4, 0.9f}, 1e-3f, make);
}

void test_for_matches_old() {
    const int n = 600;
    Inputs in;
    in.ch[0] = ramp(n, -1.f, 1.f);
    in.ch[1] = ramp(n, 1.f, -1.f);
    auto make = [] {
        auto f = std::make_shared<daisysp_ref::FormantOscillator>();
        f->Init(kSampleRate);
        return [f](const Params& p, const float* const* ins, float* o) {
            enum { kOct, kTune, kFormant, kShift };
            const float *pit = ins[0], *fr = ins[1];
            const float base = oldBaseVolts(p, kOct);
            f->SetPhaseShift(p(kShift));
            for (int i = 0; i < kBlock; i++) {
                f->SetCarrierFreq(hzClamp(voltsToHz(base + at(pit, i))));
                f->SetFormantFreq(hzClamp(p(kFormant) * fastExp2(at(fr, i)), 50.f, 12000.f));
                o[i] = f->Process() * kAudioVolts;
            }
        };
    };
    compare("for", kForType, {}, in, n, {300, 3, 0.7f}, 1e-3f, make);
}

void test_ssw_matches_old() {
    const int n = 600;
    Inputs in;
    in.ch[0] = ramp(n, -2.f, 2.f);
    in.ch[1] = ramp(n, 0.f, 2.f);
    struct Old {
        float ph[7];
        Old() {
            Rng r;
            for (float& p : ph) p = r.uni();
        }
    };
    auto make = [] {
        auto s = std::make_shared<Old>();
        return [s](const Params& p, const float* const* ins, float* o) {
            static const float kSpread[7] = {0.f, -0.11002f, 0.10929f, -0.06288f, 0.06123f, -0.01953f, 0.01953f};
            enum { kOct, kTune, kDetune, kMix };
            auto blep = [](float t, float dt) {
                if (t < dt) {
                    t /= dt;
                    return t + t - t * t - 1.f;
                }
                if (t > 1.f - dt) {
                    t = (t - 1.f) / dt;
                    return t * t + t + t + 1.f;
                }
                return 0.f;
            };
            const float *pit = ins[0], *dc = ins[1];
            const float base = oldBaseVolts(p, kOct), mix = p(kMix);
            for (int i = 0; i < kBlock; i++) {
                const float hz = hzClamp(voltsToHz(base + at(pit, i)), 1.f, 12000.f);
                const float det = clampf(p(kDetune) + at(dc, i) * 0.2f, 0.f, 1.f);
                float y = 0.f;
                for (int v = 0; v < 7; v++) {
                    const float dt = hz * (1.f + kSpread[v] * det * det) / kSampleRate;
                    s->ph[v] += dt;
                    if (s->ph[v] >= 1.f) s->ph[v] -= 1.f;
                    const float x = 2.f * s->ph[v] - 1.f - blep(s->ph[v], dt);
                    y += v == 0 ? x * (1.f - 0.5f * mix) : x * mix * 0.3f;
                }
                o[i] = clampf(y * 0.6f, -1.5f, 1.5f) * kAudioVolts;
            }
        };
    };
    // the phase steps are x * (1/sr) instead of x / sr: an ulp per step, accumulating in the
    // seven free-running phases
    compare("ssw", kSswType, {}, in, n, {300, 2, 0.9f}, 1e-3f, make);
}

void test_awh_matches_old() {
    const int n = 1200;
    Inputs in;
    in.ch[0] = noiseIn(n, 5.f);
    auto make = [] {
        auto w = std::make_shared<daisysp_ref::Autowah>();
        w->Init(kSampleRate);
        return [w](const Params& p, const float* const* ins, float* o) {
            enum { kWah, kMix, kLevel };
            const float* in = ins[0];
            w->SetWah(p(kWah));
            w->SetDryWet(p(kMix) * 100.f);
            w->SetLevel(p(kLevel));
            for (int i = 0; i < kBlock; i++) o[i] = clampf(w->Process(in ? in[i] * 0.2f : 0.f), -2.f, 2.f) * kAudioVolts;
        };
    };
    // exp2 / cos by polynomial instead of powf / cosf
    compare("awh", kAwhType, {}, in, n, {600, 0, 0.9f}, 1e-3f, make);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_svf_cache_is_exact);
    RUN_TEST(test_svf_cache_nan_and_zero);
    RUN_TEST(test_snr_matches_old);
    RUN_TEST(test_kck_matches_old);
    RUN_TEST(test_hat_matches_old);
    RUN_TEST(test_mdl_matches_old);
    RUN_TEST(test_str_matches_old);
    RUN_TEST(test_res_matches_old);
    RUN_TEST(test_vos_matches_old);
    RUN_TEST(test_zos_matches_old);
    RUN_TEST(test_for_matches_old);
    RUN_TEST(test_ssw_matches_old);
    RUN_TEST(test_awh_matches_old);
    return UNITY_END();
}
