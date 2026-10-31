#pragma once
#include <atomic>
#include <cstdint>
#include "../dsp_util.h"
#include "../module.h"
#include "common.h"

// v1.5 sequencers. Both have their own clock (BPM, RES, swing) or follow CLK, a RUN switch, and
// follow the transport (core/transport.h).
// ROL: a 32-step piano roll, one mono note per step plus ties, so a note lasts 1..32 steps.
// EVO: a line grown from SEED that mutates a little at the end of every pass.
extern const ModuleTypeInfo kRolType, kEvoType;

// ---- ROL's notes: 32 step codes of 7 bits, 3 to a param (21 bits, exact in a float) ----
namespace roll {
constexpr int kSteps = 32, kWords = 11;
constexpr int kRest = 0, kTie = 1, kBase = 62;  // a note's code = semitones above ROOT + kBase
constexpr int kMinSemi = 2 - kBase, kMaxSemi = 127 - kBase;
enum { kN0 = 0, kRoot = kWords, kScale, kOct, kBpm, kRes, kLen, kGateLen, kSwing, kGlide, kRun, kKbd, kNumParams };
enum { kInClk, kInRst, kInTrs };
enum { kOutPitch, kOutGate, kOutClk, kOutEoc };

int code(const float* words, int step);  // kRest / kTie / a note
void setCode(float* words, int step, int code);
inline bool isNote(int c) { return c >= 2; }
inline int noteCode(int semis) { return (semis < kMinSemi ? kMinSemi : (semis > kMaxSemi ? kMaxSemi : semis)) + kBase; }
// The note sounding at `step` (its start, walking back over ties), -1 = a rest there.
int noteStart(const float* words, int step);
int noteLength(const float* words, int start, int len);  // steps, ties included, up to len
// Editing (each keeps the pattern well formed: a tie always follows a note or a tie).
// A note of `n` steps at `at` (cut at len); a note it starts inside of ends before it; the rest of
// one it covers is cleared.
void writeNote(float* words, int len, int at, int n, int semis);
void rest(float* words, int len, int at, int n);    // n steps of rest
void extend(float* words, int len, int at, int n);  // the note before `at` lasts n more steps
void erase(float* words, int at);                    // the note sounding at `at` -> rests
void resize(float* words, int len, int at, int n);  // the note sounding at `at` gets n steps
// RES: steps per beat (1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T).
int stepsPerBeat(int res);
// A note value of n steps at res as text: "1/16", "1/8.", "3/4", "1" ... (<= 6 chars).
void lengthName(int n, int res, char* out, int size);
// The default pattern (a 16-step minor riff), for the params' defaults.
float defaultWord(int w);
}  // namespace roll

// ---- EVO's line ----
namespace evo {
constexpr int kMaxSteps = 16;
enum { kRoot, kScale, kOct, kBpm, kRes, kLen, kMut, kDens, kRange, kGrav, kGateLen, kSeed, kRun, kGlide, kSwing, kNumParams };
enum { kInClk, kInRst, kInTrs, kInMut };
enum { kOutPitch, kOutGate, kOutAcc, kOutClk };
enum : uint8_t { kOn = 1, kAcc = 2, kTieF = 4 };  // step flags; a tie step continues the note before
struct Line {
    int8_t deg[kMaxSteps];  // scale degrees above ROOT
    uint8_t flags[kMaxSteps];
};
// The line SEED grows into (DNS, RNG as at that moment): a random walk in the scale.
void seedLine(uint32_t seed, float dens, int range, Line& l, mods::Rng& rng);
// One pass end: every step of the first len mutates with probability mut.
void evolve(Line& l, int len, float mut, float dens, int range, float grav, mods::Rng& rng);
}  // namespace evo

// Their clock: its own steps (the module sets len at each step) or each CLK rise. RST and a
// transport start restart it: step 1 at once on its own clock, at the next rise with CLK.
// Stopped: no steps. tick() runs per sample; true = a step starts (*restart: go before step 1 first).
struct SeqClock {
    GateDetector clk, rst;
    uint32_t epoch = 0;
    bool running = true, fresh = true;
    bool started = false;  // this tick saw a transport start (not RST)
    float pos = 0.f, len = 6000.f;  // samples into the step, the step's length
    int since = 0;
    bool tick(const float* c, const float* r, int i, bool* restart);
    bool high() const { return running && pos < len * 0.5f; }  // CLK out: the step's first half
};

class RollModule : public Module {
   public:
    RollModule();
    void audition(int semis);  // UI thread: play a note above ROOT now (the roll page's keys)
    void auditionOff();
    void process(const float* const* ins, const bool* connected) override;

   private:
    void advance(const float* words, int len, bool external);
    SeqClock clk_;
    std::atomic<int> audSemis_{0};
    std::atomic<bool> audGate_{false};
    std::atomic<uint32_t> audCount_{0};
    uint32_t audSeen_ = 0;
    int step_ = -1, eoc_ = 0, gap_ = 0;
    bool note_ = false, last_ = true;
    float semis_ = 0.f, pitch_ = 0.f;
};

class EvoModule : public Module {
   public:
    EvoModule();
    // UI thread: the line now (copied unlocked: a torn copy only shows for one redraw).
    void line(evo::Line& out) const;
    uint32_t generation() const { return gen_.load(std::memory_order_relaxed); }
    void process(const float* const* ins, const bool* connected) override;

   private:
    void advance(int len, bool external);
    void reseed(uint32_t seed);
    SeqClock clk_;
    evo::Line line_{};
    mods::Rng rng_;
    std::atomic<uint32_t> gen_{0};
    uint32_t seed_ = 0xFFFFFFFFu;
    int step_ = -1;
    bool note_ = false, last_ = true, acc_ = false;
    int deg_ = 0;
    float pitch_ = 0.f, mutCv_ = 0.f;
};
