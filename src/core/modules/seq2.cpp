#include "seq2.h"
#include <cmath>
#include <cstdio>
#include "../scales.h"
#include "../transport.h"

using namespace mods;

namespace {
constexpr float kHigh = kGateHighVolts;
const char* const kOffOn[] = {"OFF", "ON"};
const char* const kResNames[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T"};

// One step of a clock at bpm and res; swing (0..1) lengthens the even steps and shortens the odd
// ones by the same amount, so a pair keeps its length (1 = 3:1).
float stepSamples(float bpm, int res, float swing, int step) {
    const float s = 60.f / clampf(bpm, 20.f, 300.f) * kSampleRate / (float)roll::stepsPerBeat(res);
    const float d = 0.5f * clampf(swing, 0.f, 1.f);
    return s * (step % 2 == 0 ? 1.f + d : 1.f - d);
}
// Exponential glide over `ms` (0 = jump).
inline float glideK(float ms) { return ms < 0.5f ? 1.f : 1.f - expf(-1.f / (ms * 0.001f * kSampleRate)); }
}  // namespace

bool SeqClock::tick(const float* c, const float* r, int i, bool* restart) {
    *restart = started = false;
    if (i == 0) {
        running = transport::running();
        const uint32_t e = transport::epoch();
        if (e != epoch) *restart = started = true;
        epoch = e;
    }
    if (r) {
        rst.process(r[i]);
        if (rst.rose) *restart = true;
    }
    if (*restart) {
        fresh = true;
        pos = 0.f;
    }
    if (c) {
        clk.process(c[i]);
        since++;
        if (!running) return false;
        if (clk.rose) {
            if (!fresh) len = (float)since;  // the step lasts one measured period
            fresh = false;
            since = 0;
            pos = 0.f;
            return true;
        }
        pos += 1.f;
        return false;
    }
    if (!running) {  // a start begins with a step
        fresh = true;
        pos = 0.f;
        return false;
    }
    if (fresh) {
        fresh = false;
        pos = 0.f;
        return true;
    }
    pos += 1.f;
    if (pos >= len) {
        pos -= len;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- ROL
namespace roll {

int code(const float* words, int step) {
    if (step < 0 || step >= kSteps) return kRest;
    return (int)(((uint32_t)words[step / 3] >> ((step % 3) * 7)) & 127u);
}

void setCode(float* words, int step, int c) {
    if (step < 0 || step >= kSteps) return;
    const int sh = (step % 3) * 7;
    uint32_t v = (uint32_t)words[step / 3];
    v = (v & ~(127u << sh)) | ((uint32_t)(c & 127) << sh);
    words[step / 3] = (float)v;
}

int noteStart(const float* words, int step) {
    for (int s = step; s >= 0; s--) {
        const int c = code(words, s);
        if (isNote(c)) return s;
        if (c != kTie) return -1;
    }
    return -1;
}

int noteLength(const float* words, int start, int len) {
    int n = 1;
    while (start + n < len && code(words, start + n) == kTie) n++;
    return n;
}

namespace {
// Ties left after `from` (the tail of a note that was cut) become rests.
void clearTail(float* words, int from) {
    for (int s = from; s < kSteps && code(words, s) == kTie; s++) setCode(words, s, kRest);
}
int clip(int at, int n, int len) { return at + n > len ? len - at : n; }
}  // namespace

void writeNote(float* words, int len, int at, int n, int semis) {
    if (at < 0 || at >= len || n < 1) return;
    n = clip(at, n, len);
    setCode(words, at, noteCode(semis));
    for (int s = at + 1; s < at + n; s++) setCode(words, s, kTie);
    clearTail(words, at + n);
}

void rest(float* words, int len, int at, int n) {
    if (at < 0 || at >= len || n < 1) return;
    n = clip(at, n, len);
    for (int s = at; s < at + n; s++) setCode(words, s, kRest);
    clearTail(words, at + n);
}

void extend(float* words, int len, int at, int n) {
    if (at < 1 || at >= len || n < 1 || noteStart(words, at - 1) < 0) return;
    n = clip(at, n, len);
    for (int s = at; s < at + n; s++) setCode(words, s, kTie);
    clearTail(words, at + n);
}

void erase(float* words, int at) {
    const int s0 = noteStart(words, at);
    if (s0 < 0) return;
    setCode(words, s0, kRest);
    clearTail(words, s0 + 1);
}

void resize(float* words, int len, int at, int n) {
    const int s0 = noteStart(words, at);
    if (s0 < 0) return;
    writeNote(words, len, s0, n, code(words, s0) - kBase);
}

int stepsPerBeat(int res) {
    static const int k[] = {1, 2, 4, 8, 3, 6};
    return k[res < 0 ? 0 : (res > 5 ? 5 : res)];
}

void lengthName(int n, int res, char* out, int size) {
    int a = n < 1 ? 1 : n, b = 4 * stepsPerBeat(res);  // a/b of a whole note
    for (int g = a < b ? a : b; g > 1; g--)
        if (a % g == 0 && b % g == 0) {
            a /= g;
            b /= g;
            break;
        }
    if (a == b) snprintf(out, (size_t)size, "1");
    else if (a == 1) snprintf(out, (size_t)size, "1/%d", b);
    else if (a == 3 && b % 2 == 0) snprintf(out, (size_t)size, "1/%d.", b / 2);
    else snprintf(out, (size_t)size, "%d/%d", a, b);
}

namespace {
// The default riff (C minor, 16 steps): note codes, T = tie, R = rest.
constexpr int T = kTie, R = kRest;
constexpr int N(int semis) { return semis + kBase; }
constexpr int kRiff[kSteps] = {N(0), T, R, N(0), N(3), T, N(7), N(10), N(12), T, T, N(7), N(5), T, N(3), N(2),
                               R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R};
constexpr float word(int w) {
    uint32_t v = 0;
    for (int k = 0; k < 3; k++)
        if (w * 3 + k < kSteps) v |= (uint32_t)kRiff[w * 3 + k] << (k * 7);
    return (float)v;
}
}  // namespace
float defaultWord(int w) { return w >= 0 && w < kWords ? word(w) : 0.f; }

#define WORD(n) {"n" #n, "N" #n, 0.f, 2097151.f, word(n), Curve::Stepped, Unit::Int, nullptr}
const ParamSpec kParams[] = {
    WORD(0), WORD(1), WORD(2), WORD(3), WORD(4), WORD(5), WORD(6), WORD(7), WORD(8), WORD(9), WORD(10),
    {"root", "ROT", 0.f, 11.f, 0.f, Curve::Stepped, Unit::Enum, kNoteNames},
    {"scale", "SCL", 0.f, 12.f, 2.f, Curve::Stepped, Unit::Enum, kScaleNames},
    {"oct", "OCT", -3.f, 3.f, -1.f, Curve::Stepped, Unit::Oct, nullptr},
    {"bpm", "BPM", 20.f, 300.f, 120.f, Curve::Log, Unit::Bpm, nullptr},
    {"res", "RES", 0.f, 5.f, 2.f, Curve::Stepped, Unit::Enum, kResNames},
    {"len", "LEN", 1.f, 32.f, 16.f, Curve::Stepped, Unit::Int, nullptr},
    {"gatelen", "GLN", 0.05f, 1.f, 0.7f, Curve::Lin, Unit::Percent, nullptr},
    {"swing", "SWG", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
    {"glide", "GLD", 0.f, 500.f, 0.f, Curve::Sqr, Unit::Ms, nullptr},
    {"run", "RUN", 0.f, 1.f, 1.f, Curve::Stepped, Unit::Enum, kOffOn},
    {"kbd", "KBD", -3.f, 3.f, 0.f, Curve::Stepped, Unit::Oct, nullptr},
};
#undef WORD
static_assert(sizeof(kParams) / sizeof(kParams[0]) == kNumParams, "ROL params");
const char* const kInKeys[] = {"clk", "rst", "trs"};
const char* const kInLabels[] = {"CLK", "RST", "TRS"};
const char* const kOutKeys[] = {"pitch", "gate", "clk", "eoc"};
const char* const kOutLabels[] = {"PIT", "GAT", "CLK", "EOC"};
const CellRef kSetCells[36] = {
    cP(kRoot), cP(kScale), cP(kOct), cP(kBpm), cP(kRun), cD, cD, cD, cD, cD, cD, cD,
    cP(kRes), cP(kLen), cP(kGateLen), cP(kSwing), cP(kGlide), cD, cD, cD, cD, cD, cD, cD,
    cP(kKbd), c_, c_, c_, c_, cD, cD, cD, cD, cD, cD, cD,
};
const CellRef kPinCells[36] = {
    cI(kInClk), cI(kInRst), cI(kInTrs), c_, c_, c_, c_, c_, c_, c_, c_, c_,
    cO(kOutPitch), cO(kOutGate), cO(kOutClk), cO(kOutEoc), c_, c_, c_, c_, c_, c_, c_, c_,
    c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const PageDef kPages[] = {
    {"ROLL", PageKind::Play, Drawing::None, nullptr},
    {"SET", PageKind::Cells, Drawing::Picture, kSetCells},
    {"PINS", PageKind::Cells, Drawing::None, kPinCells},
};
Module* create() { return new RollModule(); }
}  // namespace roll

RollModule::RollModule() : Module(kRolType) { clk_.epoch = transport::epoch(); }

void RollModule::audition(int semis) {
    audSemis_.store(semis, std::memory_order_relaxed);
    audGate_.store(true, std::memory_order_relaxed);
    audCount_.fetch_add(1, std::memory_order_release);
}

void RollModule::auditionOff() { audGate_.store(false, std::memory_order_release); }

void RollModule::advance(const float* words, int len, bool external) {
    using namespace roll;
    step_ = (step_ + 1) % len;
    if (step_ == 0) eoc_ = 48;  // 1 ms
    if (!external) clk_.len = stepSamples(param(kBpm), (int)param(kRes), param(kSwing), step_);
    const int c = code(words, step_);
    if (isNote(c)) {
        note_ = true;
        semis_ = (float)(c - kBase);
    } else if (c != kTie) {
        note_ = false;  // a rest (a tie with nothing before it plays nothing either)
    }
    last_ = !(step_ + 1 < len && code(words, step_ + 1) == kTie);
}

void RollModule::process(const float* const* ins, const bool* c) {
    using namespace roll;
    const float* ck = inOrNull(ins, c, kInClk);
    const float* rs = inOrNull(ins, c, kInRst);
    const float* trs = inOrNull(ins, c, kInTrs);
    float words[kWords];
    for (int w = 0; w < kWords; w++) words[w] = param(kN0 + w);
    int len = (int)param(kLen);
    len = len < 1 ? 1 : (len > kSteps ? kSteps : len);
    const float gl = param(kGateLen), k = glideK(param(kGlide));
    const bool run = param(kRun) >= 0.5f;
    const int scale = (int)param(kScale);
    const float base = (param(kRoot) + 12.f * param(kOct)) / 12.f;
    const uint32_t aud = audCount_.load(std::memory_order_acquire);
    const bool audGate = audGate_.load(std::memory_order_relaxed);
    if (aud != audSeen_) {
        audSeen_ = aud;
        if (out(kOutGate)[kBlock - 1] > 0.f) gap_ = 48;  // a new key on a high gate: retrigger
    }
    const float audPitch = base + quantize((float)audSemis_.load(std::memory_order_relaxed) / 12.f, scale, 0);
    if (step_ >= len) step_ = len - 1;  // LEN shrank under the playhead: wrap at the next step
    float target = base + quantize(semis_ / 12.f, scale, 0);
    for (int i = 0; i < kBlock; i++) {
        bool restart;
        const bool tick = clk_.tick(ck, rs, i, &restart);
        if (restart) {
            step_ = -1;
            note_ = false;
        }
        if (tick) {
            advance(words, len, ck != nullptr);
            target = base + quantize(semis_ / 12.f, scale, 0);
        }
        const bool playing = clk_.running && step_ >= 0;
        bool gate = playing && run && note_ && (!last_ || clk_.pos < gl * clk_.len);
        float p = target;
        if (audGate) {  // a roll-page key plays over the pattern
            gate = true;
            p = audPitch;
        }
        if (gap_ > 0) {
            gap_--;
            gate = false;
        }
        pitch_ += k * (p - pitch_);
        out(kOutPitch)[i] = pitch_ + at(trs, i);
        out(kOutGate)[i] = gate ? kHigh : 0.f;
        out(kOutClk)[i] = playing && clk_.high() ? kHigh : 0.f;
        out(kOutEoc)[i] = eoc_ > 0 ? (eoc_--, kHigh) : 0.f;
    }
    setMonitor(0, (float)step_);
    setMonitor(1, out(kOutGate)[kBlock - 1] > 0.f ? 1.f : 0.f);
}

const ModuleTypeInfo kRolType = {
    "rol", "ROL", "Piano roll: notes of any length", Category::Modulator, Icon::Seq, Drawing::None,
    roll::kNumParams, roll::kParams, 3, roll::kInKeys, roll::kInLabels, 4, roll::kOutKeys, roll::kOutLabels,
    90, sizeof(RollModule) + 4 * kBlock * sizeof(float), 0, roll::create, roll::kPages, 3};

// ---------------------------------------------------------------- EVO
namespace evo {

namespace {
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

void seedLine(uint32_t seed, float dens, int range, Line& l, Rng& rng) {
    rng.s = seed * 2654435761u + 0x9E3779B9u;
    for (int k = 0; k < 4; k++) rng.next();
    int d = 0;
    for (int i = 0; i < kMaxSteps; i++) {
        if (i > 0) {
            const float r = rng.uni();
            const int step = r < 0.3f ? 0 : (r < 0.75f ? 1 : (r < 0.93f ? 2 : 3));
            int dir = rng.uni() < 0.5f ? -1 : 1;
            if (d + dir * step > range || d + dir * step < -range) dir = -dir;
            d = clampi(d + dir * step, -range, range);
        }
        l.deg[i] = (int8_t)d;
        uint8_t f = 0;
        if (i == 0 || rng.uni() < dens) f |= kOn;
        if ((f & kOn) && i > 0 && (l.flags[i - 1] & kOn) && rng.uni() < 0.15f) f |= kTieF;
        if (rng.uni() < 0.2f) f |= kAcc;
        l.flags[i] = f;
    }
}

void evolve(Line& l, int len, float mut, float dens, int range, float grav, Rng& rng) {
    len = clampi(len, 1, kMaxSteps);
    for (int i = 0; i < len; i++) {
        if (rng.uni() >= mut) continue;
        const float r = rng.uni();
        uint8_t& f = l.flags[i];
        if (r < 0.45f) {  // the pitch walks a degree or two, pulled home by GRV
            const int by = rng.uni() < 0.7f ? 1 : 2;
            int dir = rng.uni() < 0.5f ? -1 : 1;
            if (l.deg[i] != 0 && rng.uni() < grav) dir = l.deg[i] > 0 ? -1 : 1;
            l.deg[i] = (int8_t)clampi(l.deg[i] + dir * by, -range, range);
            f &= (uint8_t)~kTieF;  // a moved note is heard
        } else if (r < 0.7f) {  // a note comes or goes, as DNS likes
            if (rng.uni() < dens) f |= kOn;
            else f &= (uint8_t)~(kOn | kTieF);
        } else if (r < 0.82f) {  // tie it to the note before, or cut it loose
            if (i > 0 && (f & kOn) && (l.flags[i - 1] & kOn)) f ^= kTieF;
        } else if (r < 0.92f) {  // echo the step half a pass away: motifs repeat
            const int j = (i + len / 2) % len;
            l.deg[i] = l.deg[j];
            f = (uint8_t)(l.flags[j] & (kOn | kAcc));
        } else {
            f ^= kAcc;
        }
    }
    for (int i = 0; i < len; i++)
        if (l.flags[i] & kOn) return;
    l.flags[0] |= kOn;  // never silent for good
    l.flags[0] &= (uint8_t)~kTieF;
}

const ParamSpec kParams[] = {
    {"root", "ROT", 0.f, 11.f, 0.f, Curve::Stepped, Unit::Enum, kNoteNames},
    {"scale", "SCL", 0.f, 12.f, 10.f, Curve::Stepped, Unit::Enum, kScaleNames},
    {"oct", "OCT", -3.f, 3.f, 0.f, Curve::Stepped, Unit::Oct, nullptr},
    {"bpm", "BPM", 20.f, 300.f, 120.f, Curve::Log, Unit::Bpm, nullptr},
    {"res", "RES", 0.f, 5.f, 2.f, Curve::Stepped, Unit::Enum, kResNames},
    {"len", "LEN", 2.f, 16.f, 16.f, Curve::Stepped, Unit::Int, nullptr},
    {"mut", "MUT", 0.f, 1.f, 0.2f, Curve::Lin, Unit::Percent, nullptr},
    {"dens", "DNS", 0.f, 1.f, 0.6f, Curve::Lin, Unit::Percent, nullptr},
    {"range", "RNG", 1.f, 14.f, 7.f, Curve::Stepped, Unit::Int, nullptr},
    {"grav", "GRV", 0.f, 1.f, 0.4f, Curve::Lin, Unit::Percent, nullptr},
    {"gatelen", "GLN", 0.05f, 1.f, 0.5f, Curve::Lin, Unit::Percent, nullptr},
    {"seed", "SED", 0.f, 999.f, 1.f, Curve::Stepped, Unit::Int, nullptr},
    {"run", "RUN", 0.f, 1.f, 1.f, Curve::Stepped, Unit::Enum, kOffOn},
    {"glide", "GLD", 0.f, 500.f, 0.f, Curve::Sqr, Unit::Ms, nullptr},
    {"swing", "SWG", 0.f, 1.f, 0.f, Curve::Lin, Unit::Percent, nullptr},
};
static_assert(sizeof(kParams) / sizeof(kParams[0]) == kNumParams, "EVO params");
const char* const kInKeys[] = {"clk", "rst", "trs", "mut"};
const char* const kInLabels[] = {"CLK", "RST", "TRS", "MUT"};
const char* const kOutKeys[] = {"pitch", "gate", "acc", "clk"};
const char* const kOutLabels[] = {"PIT", "GAT", "ACC", "CLK"};
const CellRef kMainCells[36] = {
    cP(kMut), cP(kDens), cP(kRange), cP(kGrav), cP(kSeed), cD, cD, cD, cD, cD, cD, cD,
    cP(kRoot), cP(kScale), cP(kOct), cP(kLen), cP(kRun), cD, cD, cD, cD, cD, cD, cD,
    cP(kBpm), cP(kRes), cP(kGateLen), cP(kGlide), cP(kSwing), cD, cD, cD, cD, cD, cD, cD,
};
const CellRef kPinCells[36] = {
    cI(kInClk), cI(kInRst), cI(kInTrs), cI(kInMut), c_, c_, c_, c_, c_, c_, c_, c_,
    cO(kOutPitch), cO(kOutGate), cO(kOutAcc), cO(kOutClk), c_, c_, c_, c_, c_, c_, c_, c_,
    c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_, c_,
};
const PageDef kPages[] = {
    {"LINE", PageKind::Cells, Drawing::Picture, kMainCells},
    {"PINS", PageKind::Cells, Drawing::None, kPinCells},
};
Module* create() { return new EvoModule(); }
}  // namespace evo

EvoModule::EvoModule() : Module(kEvoType) { clk_.epoch = transport::epoch(); }

void EvoModule::line(evo::Line& out) const { out = line_; }

void EvoModule::reseed(uint32_t seed) {
    seed_ = seed;
    evo::seedLine(seed, param(evo::kDens), (int)param(evo::kRange), line_, rng_);
    gen_.store(0, std::memory_order_relaxed);
}

void EvoModule::advance(int len, bool external) {
    using namespace evo;
    const bool wrap = step_ == len - 1 || step_ >= len;
    step_ = step_ < 0 || wrap ? 0 : step_ + 1;
    if (wrap) {  // a pass ended: the line grows
        const float mut = clampf(param(kMut) + mutCv_, 0.f, 1.f);
        evolve(line_, len, mut, param(kDens), (int)param(kRange), param(kGrav), rng_);
        gen_.fetch_add(1, std::memory_order_relaxed);
    }
    if (!external) clk_.len = stepSamples(param(kBpm), (int)param(kRes), param(kSwing), step_);
    const uint8_t f = line_.flags[step_];
    if (f & kOn) {
        if (!((f & kTieF) && note_)) {  // a tie keeps the note before sounding
            deg_ = line_.deg[step_];
            acc_ = (f & kAcc) != 0;
        }
        note_ = true;
    } else {
        note_ = false;
    }
    const int nx = step_ + 1 < len ? step_ + 1 : 0;
    last_ = !((line_.flags[nx] & kOn) && (line_.flags[nx] & kTieF) && nx != 0);
}

void EvoModule::process(const float* const* ins, const bool* c) {
    using namespace evo;
    const float* ck = inOrNull(ins, c, kInClk);
    const float* rs = inOrNull(ins, c, kInRst);
    const float* trs = inOrNull(ins, c, kInTrs);
    const float* mc = inOrNull(ins, c, kInMut);
    mutCv_ = mc ? mc[0] / 5.f : 0.f;
    const uint32_t seed = (uint32_t)param(kSeed);
    if (seed != seed_) reseed(seed);
    int len = (int)param(kLen);
    len = len < 1 ? 1 : (len > kMaxSteps ? kMaxSteps : len);
    const int range = (int)param(kRange), scale = (int)param(kScale);
    const float gl = param(kGateLen), k = glideK(param(kGlide));
    const bool run = param(kRun) >= 0.5f;
    const float base = (param(kRoot) + 12.f * param(kOct)) / 12.f;
    auto pitchOf = [&](int d) {
        d = d < -range ? -range : (d > range ? range : d);
        return base + (float)scaleDegree(scale, d) / 12.f;
    };
    float target = pitchOf(deg_);
    for (int i = 0; i < kBlock; i++) {
        bool restart;
        const bool tick = clk_.tick(ck, rs, i, &restart);
        if (restart) {
            step_ = -1;
            note_ = false;
            if (clk_.started) reseed(seed);  // a start replays the seed's line (RST only rewinds)
        }
        if (tick) {
            advance(len, ck != nullptr);
            target = pitchOf(deg_);
        }
        const bool playing = clk_.running && step_ >= 0;
        const bool gate = playing && run && note_ && (!last_ || clk_.pos < gl * clk_.len);
        pitch_ += k * (target - pitch_);
        out(kOutPitch)[i] = pitch_ + at(trs, i);
        out(kOutGate)[i] = gate ? kHigh : 0.f;
        out(kOutAcc)[i] = gate && acc_ ? kHigh : 0.f;
        out(kOutClk)[i] = playing && clk_.high() ? kHigh : 0.f;
    }
    setMonitor(0, (float)step_);
    setMonitor(1, (float)gen_.load(std::memory_order_relaxed));
}

const ModuleTypeInfo kEvoType = {
    "evo", "EVO", "Evolving line: mutates each pass", Category::Modulator, Icon::Seq, Drawing::Picture,
    evo::kNumParams, evo::kParams, 4, evo::kInKeys, evo::kInLabels, 4, evo::kOutKeys, evo::kOutLabels,
    90, sizeof(EvoModule) + 4 * kBlock * sizeof(float), 0, evo::create, evo::kPages, 2};
