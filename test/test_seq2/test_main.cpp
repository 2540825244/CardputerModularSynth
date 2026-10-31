// v1.5: transport, ROL (piano roll), EVO (evolving line), SEQ root/oct.
#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "core/modules/clk.h"
#include "core/modules/mod.h"
#include "core/modules/seq2.h"
#include "core/scales.h"
#include "core/transport.h"

void setUp() { transport::play(); }
void tearDown() {}

namespace {
struct Run {
    std::vector<float> pit, gate, o2, o3;
};
// Runs n blocks with no inputs patched (own clock).
Run run(Module& m, int blocks, const float* const* ins = nullptr, const bool* conn = nullptr) {
    static const bool none[kMaxIns] = {};
    static const float* nul[kMaxIns] = {};
    Run r;
    for (int b = 0; b < blocks; b++) {
        m.process(ins ? ins : nul, conn ? conn : none);
        r.pit.insert(r.pit.end(), m.out(0), m.out(0) + kBlock);
        r.gate.insert(r.gate.end(), m.out(1), m.out(1) + kBlock);
        r.o2.insert(r.o2.end(), m.out(2), m.out(2) + kBlock);
        r.o3.insert(r.o3.end(), m.out(3), m.out(3) + kBlock);
    }
    return r;
}
struct Note {
    int start, len;
    float pitch;
};
std::vector<Note> notes(const Run& r) {
    std::vector<Note> v;
    for (size_t i = 0; i < r.gate.size(); i++) {
        const bool on = r.gate[i] > 2.5f, was = i > 0 && r.gate[i - 1] > 2.5f;
        if (on && !was) v.push_back({(int)i, 0, r.pit[i]});
        if (on) v.back().len++;
    }
    return v;
}
void setAll(Module& m, const ModuleTypeInfo& t) {
    for (int p = 0; p < t.numParams; p++) m.setParam(p, t.params[p].def);
}
float words0[roll::kWords];
}  // namespace

// ---- the roll's packed steps ----
void test_roll_codes_pack_three_a_word() {
    float w[roll::kWords] = {};
    for (int s = 0; s < roll::kSteps; s++) roll::setCode(w, s, (s * 37 + 5) & 127);
    for (int s = 0; s < roll::kSteps; s++) TEST_ASSERT_EQUAL((s * 37 + 5) & 127, roll::code(w, s));
    for (float x : w) TEST_ASSERT_TRUE(x >= 0.f && x <= 2097151.f && x == floorf(x));
}

void test_roll_write_cuts_and_cleans() {
    float w[roll::kWords] = {};
    roll::writeNote(w, 16, 0, 4, 0);  // C for 4 steps
    TEST_ASSERT_EQUAL(4, roll::noteLength(w, 0, 16));
    roll::writeNote(w, 16, 2, 1, 7);  // G inside it: C now 2 steps, G 1 step, the C's tail is cleared
    TEST_ASSERT_EQUAL(2, roll::noteLength(w, 0, 16));
    TEST_ASSERT_EQUAL(roll::noteCode(7), roll::code(w, 2));
    TEST_ASSERT_EQUAL(roll::kRest, roll::code(w, 3));
    TEST_ASSERT_EQUAL(0, roll::noteStart(w, 1));
    TEST_ASSERT_EQUAL(-1, roll::noteStart(w, 3));
    roll::writeNote(w, 16, 14, 8, 3);  // clipped at LEN
    TEST_ASSERT_EQUAL(2, roll::noteLength(w, 14, 16));
    TEST_ASSERT_EQUAL(roll::kRest, roll::code(w, 16));
    roll::resize(w, 16, 1, 6);  // the note sounding at step 1 (the C) gets 6 steps, over the G
    TEST_ASSERT_EQUAL(6, roll::noteLength(w, 0, 16));
    TEST_ASSERT_EQUAL(roll::noteCode(0), roll::code(w, 0));
    roll::extend(w, 16, 6, 2);
    TEST_ASSERT_EQUAL(8, roll::noteLength(w, 0, 16));
    roll::erase(w, 5);
    for (int s = 0; s < 10; s++) TEST_ASSERT_EQUAL(roll::kRest, roll::code(w, s));
    roll::rest(w, 16, 14, 1);  // a rest cuts the head of the last note; its tie becomes a rest
    TEST_ASSERT_EQUAL(roll::kRest, roll::code(w, 15));
}

void test_roll_length_names() {
    char s[8];
    roll::lengthName(1, 2, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1/16", s);
    roll::lengthName(3, 2, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1/8.", s);
    roll::lengthName(4, 2, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1/4", s);
    roll::lengthName(12, 2, s, sizeof s);  // 3/4 = a dotted half
    TEST_ASSERT_EQUAL_STRING("1/2.", s);
    roll::lengthName(5, 2, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("5/16", s);
    roll::lengthName(16, 2, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1", s);
    roll::lengthName(1, 4, s, sizeof s);  // an 8th triplet
    TEST_ASSERT_EQUAL_STRING("1/12", s);
}

// ---- ROL playing ----
void test_rol_default_riff_timing_and_lengths() {
    RollModule m;
    setAll(m, kRolType);
    // 120 BPM, 1/16: a step is 6000 samples. The riff: C(2) rest C(1) Eb(2) G(1) Bb(1) C5(3) G(1) F(2) Eb(1) D(1)
    const Run r = run(m, 16 * 6000 / kBlock + 10);
    const std::vector<Note> n = notes(r);
    TEST_ASSERT_TRUE(n.size() >= 10);
    const int steps[] = {0, 3, 4, 6, 7, 8, 11, 12, 14, 15};
    const int lens[] = {2, 1, 2, 1, 1, 3, 1, 2, 1, 1};
    const int semis[] = {0, 0, 3, 7, 10, 12, 7, 5, 3, 2};
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_INT_WITHIN(2, steps[i] * 6000, n[i].start);
        // gate: (len - 1) steps + GLN 70 % of the last
        TEST_ASSERT_INT_WITHIN(40, (int)((lens[i] - 1 + 0.7f) * 6000), n[i].len);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.f + semis[i] / 12.f, n[i].pitch);  // OCT -1, C minor
    }
}

void test_rol_root_and_scale_transpose() {
    RollModule m;
    setAll(m, kRolType);
    m.setParam(roll::kRoot, 2.f);   // D
    m.setParam(roll::kOct, 0.f);
    m.setParam(roll::kScale, 1.f);  // major: the riff's Eb (3) snaps up to E (4), Bb (10) to B (11)
    const std::vector<Note> n = notes(run(m, 8 * 6000 / kBlock + 10));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.f / 12.f, n[0].pitch);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, (2.f + 4.f) / 12.f, n[2].pitch);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, (2.f + 11.f) / 12.f, n[4].pitch);
}

void test_rol_run_off_mutes_but_keeps_time() {
    RollModule m;
    setAll(m, kRolType);
    m.setParam(roll::kRun, 0.f);
    const Run r = run(m, 4 * 6000 / kBlock);
    TEST_ASSERT_EQUAL(0, (int)notes(r).size());
    TEST_ASSERT_TRUE(m.monitor(0) >= 3.f);  // it counted on
    int clocks = 0;
    for (size_t i = 0; i < r.o2.size(); i++) clocks += r.o2[i] > 2.5f && (i == 0 || r.o2[i - 1] < 2.5f);
    TEST_ASSERT_EQUAL(4, clocks);  // CLK out ticks every step
}

void test_rol_transport_stop_and_restart() {
    RollModule m;
    setAll(m, kRolType);
    run(m, 5 * 6000 / kBlock);
    transport::stop();
    const Run quiet = run(m, 100);
    for (float g : quiet.gate) TEST_ASSERT_EQUAL_FLOAT(0.f, g);
    for (float c : quiet.o2) TEST_ASSERT_EQUAL_FLOAT(0.f, c);
    transport::play();
    const Run r = run(m, 10);
    TEST_ASSERT_TRUE(r.gate[0] > 2.5f);  // step 1 at once
    TEST_ASSERT_EQUAL_FLOAT(0.f, m.monitor(0));
    TEST_ASSERT_TRUE(r.o3[0] > 2.5f);  // EOC: the pattern (re)starts
}

void test_rol_external_clock_steps() {
    RollModule m;
    setAll(m, kRolType);
    static float ck[kBlock];
    const float* ins[3] = {ck, nullptr, nullptr};
    const bool conn[3] = {true, false, false};
    std::vector<float> gate;
    for (int b = 0; b < 400; b++) {  // a rise every 50 blocks (1600 samples)
        for (int i = 0; i < kBlock; i++) ck[i] = (b % 50) < 25 ? 5.f : 0.f;
        m.process(ins, conn);
        gate.insert(gate.end(), m.out(1), m.out(1) + kBlock);
    }
    TEST_ASSERT_EQUAL_FLOAT(7.f, m.monitor(0));  // 8 rises: steps 0..7
    TEST_ASSERT_TRUE(gate[10] > 2.5f);             // step 0 is a note
}

void test_rol_audition_plays_over() {
    RollModule m;
    setAll(m, kRolType);
    transport::stop();
    run(m, 2);
    m.audition(7);
    Run r = run(m, 4);
    TEST_ASSERT_TRUE(r.gate.back() > 2.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.f + 7.f / 12.f, r.pit.back());
    m.auditionOff();
    r = run(m, 2);
    TEST_ASSERT_EQUAL_FLOAT(0.f, r.gate.back());
}

void test_rol_swing_keeps_pairs() {
    RollModule m;
    setAll(m, kRolType);
    for (int w = 0; w < roll::kWords; w++) m.setParam(w, 0.f);
    float w[roll::kWords] = {};
    for (int s = 0; s < 4; s++) roll::writeNote(w, 16, s, 1, 0);
    for (int i = 0; i < roll::kWords; i++) m.setParam(i, w[i]);
    m.setParam(roll::kSwing, 0.5f);  // even steps 1.25x, odd 0.75x
    const std::vector<Note> n = notes(run(m, 4 * 6000 / kBlock + 10));
    TEST_ASSERT_INT_WITHIN(2, 7500, n[1].start);
    TEST_ASSERT_INT_WITHIN(2, 12000, n[2].start);
}

// ---- EVO ----
void test_evo_same_seed_same_line_and_in_range() {
    evo::Line a, b;
    mods::Rng r1, r2;
    evo::seedLine(42, 0.6f, 5, a, r1);
    evo::seedLine(42, 0.6f, 5, b, r2);
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);
    TEST_ASSERT_TRUE(a.flags[0] & evo::kOn);
    for (int k = 0; k < 50; k++) evo::evolve(a, 16, 0.5f, 0.6f, 5, 0.4f, r1);
    for (int i = 0; i < evo::kMaxSteps; i++) TEST_ASSERT_TRUE(a.deg[i] >= -5 && a.deg[i] <= 5);
    bool any = false;
    for (int i = 0; i < 16; i++) any = any || (a.flags[i] & evo::kOn);
    TEST_ASSERT_TRUE(any);
}

void test_evo_mut_zero_freezes() {
    evo::Line a, b;
    mods::Rng r;
    evo::seedLine(7, 0.6f, 7, a, r);
    b = a;
    for (int k = 0; k < 20; k++) evo::evolve(a, 16, 0.f, 0.6f, 7, 0.4f, r);
    TEST_ASSERT_EQUAL_MEMORY(&b, &a, sizeof a);
    for (int k = 0; k < 20; k++) evo::evolve(a, 16, 0.3f, 0.6f, 7, 0.4f, r);
    TEST_ASSERT_TRUE(memcmp(&a, &b, sizeof a) != 0);
}

void test_evo_plays_in_scale_and_evolves() {
    EvoModule m;
    setAll(m, kEvoType);
    const int pass = 16 * 6000;
    const Run r = run(m, 4 * pass / kBlock);
    const std::vector<Note> n = notes(r);
    TEST_ASSERT_TRUE(n.size() >= 8);
    const unsigned mask = scaleMask(10);  // minor pentatonic from C
    for (const Note& x : n) {
        const int semi = (int)lroundf(x.pitch * 12.f);
        TEST_ASSERT_TRUE_MESSAGE((mask >> (((semi % 12) + 12) % 12)) & 1u, "out of scale");
    }
    TEST_ASSERT_TRUE(m.generation() >= 3);
}

void test_evo_start_replays_the_seed() {
    EvoModule a;
    setAll(a, kEvoType);
    a.setParam(evo::kMut, 0.8f);
    const Run first = run(a, 3 * 16 * 6000 / kBlock);
    transport::play();  // a new take
    const Run again = run(a, 3 * 16 * 6000 / kBlock);
    TEST_ASSERT_EQUAL(first.gate.size(), again.gate.size());
    TEST_ASSERT_EQUAL_MEMORY(first.gate.data(), again.gate.data(), first.gate.size() * sizeof(float));
}

// ---- SEQ root / oct, CLK and the transport ----
void test_seq_root_and_octave() {
    Module* m = kSeqType.create();
    for (int p = 0; p < kSeqType.numParams; p++) m->setParam(p, kSeqType.params[p].def);
    m->setParam(0, 3.f);    // step 1: +3 semitones (chromatic)
    m->setParam(16, 5.f);   // ROOT F
    m->setParam(17, -1.f);  // OCT -1
    static float ck[kBlock];
    for (float& v : ck) v = 5.f;
    const float* ins[2] = {ck, nullptr};
    const bool conn[2] = {true, false};
    m->process(ins, conn);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, (5.f + 3.f - 12.f) / 12.f, m->out(0)[kBlock - 1]);
    delete m;
}

void test_clk_follows_transport() {
    Module* m = kClkType.create();
    m->setParam(0, 120.f);
    m->setParam(1, 0.f);
    static const bool none[2] = {};
    const float* nul[2] = {};
    for (int b = 0; b < 300; b++) m->process(nul, none);
    transport::stop();
    for (int b = 0; b < 10; b++) {
        m->process(nul, none);
        for (int i = 0; i < kBlock; i++) TEST_ASSERT_EQUAL_FLOAT(0.f, m->out(0)[i]);
    }
    transport::play();
    m->process(nul, none);
    TEST_ASSERT_TRUE(m->out(0)[0] > 2.5f);  // the beat restarts at once
    delete m;
}

void test_seq_restarts_with_the_transport() {
    Module* clk = kClkType.create();
    Module* seq = kSeqType.create();
    for (int p = 0; p < kSeqType.numParams; p++) seq->setParam(p, kSeqType.params[p].def);
    clk->setParam(0, 120.f);
    static const bool none[2] = {};
    const float* nul[2] = {};
    const float* ins[2] = {clk->out(0), nullptr};
    const bool conn[2] = {true, false};
    for (int b = 0; b < 2000; b++) {  // a few beats in
        clk->process(nul, none);
        seq->process(ins, conn);
    }
    TEST_ASSERT_TRUE(seq->monitor(0) > 0.f);
    transport::play();
    clk->process(nul, none);
    seq->process(ins, conn);
    TEST_ASSERT_EQUAL_FLOAT(0.f, seq->monitor(0));  // back on step 1 with the clock's first beat
    delete clk;
    delete seq;
}

// ---- review fixes ----
void test_seq_restart_keeps_separate_gates() {  // a stop / play must not stretch step 1 into step 2
    Module* clk = kClkType.create();
    Module* seq = kSeqType.create();
    for (int p = 0; p < kSeqType.numParams; p++) seq->setParam(p, kSeqType.params[p].def);
    clk->setParam(0, 120.f);
    static const bool none[2] = {};
    const float* nul[2] = {};
    const float* ins[2] = {clk->out(0), nullptr};
    const bool conn[2] = {true, false};
    for (int b = 0; b < 1000; b++) {
        clk->process(nul, none);
        seq->process(ins, conn);
    }
    transport::stop();
    for (int b = 0; b < 3000; b++) {  // 2 s stopped
        clk->process(nul, none);
        seq->process(ins, conn);
    }
    transport::play();
    int rises = 0;
    bool was = false;
    for (int b = 0; b < 24000 / kBlock + 10; b++) {  // a beat is 24000 samples: steps 1 and 2
        clk->process(nul, none);
        seq->process(ins, conn);
        for (int i = 0; i < kBlock; i++) {
            const bool on = seq->out(1)[i] > 2.5f;
            rises += on && !was;
            was = on;
        }
    }
    TEST_ASSERT_EQUAL(2, rises);
    delete clk;
    delete seq;
}

void test_rol_restart_while_running_is_a_fresh_edge() {
    RollModule m;
    setAll(m, kRolType);
    run(m, 50);  // inside step 0 (1600 of 6000 samples): its gate and CLK out are high
    TEST_ASSERT_TRUE(m.out(1)[kBlock - 1] > 2.5f);
    TEST_ASSERT_TRUE(m.out(2)[kBlock - 1] > 2.5f);
    transport::play();
    const Run r = run(m, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.f, r.gate[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.f, r.o2[0]);
    TEST_ASSERT_TRUE(r.gate[1] > 2.5f && r.o2[1] > 2.5f);
}

void test_rol_swing_odd_len_keeps_tempo() {
    RollModule m;
    setAll(m, kRolType);
    m.setParam(roll::kLen, 3.f);
    m.setParam(roll::kSwing, 0.5f);
    const Run r = run(m, 30 * 6000 / kBlock + 4);
    int clocks = 0;
    for (size_t i = 0; i < r.o2.size(); i++) clocks += r.o2[i] > 2.5f && (i == 0 || r.o2[i - 1] < 2.5f);
    TEST_ASSERT_EQUAL(31, clocks);  // 30 steps' time holds 30 (+1 starting) steps: no drift
}

void test_rol_tempo_change_takes_the_step_playing() {
    RollModule m;
    setAll(m, kRolType);
    m.setParam(roll::kRes, 0.f);  // quarters: a step is 24000 samples
    m.setParam(roll::kBpm, 20.f); // ... 144000 at 20 BPM
    run(m, 10);
    m.setParam(roll::kBpm, 120.f);
    run(m, 24000 / kBlock + 2);
    TEST_ASSERT_EQUAL_FLOAT(1.f, m.monitor(0));  // step 2 came at the new tempo
}

int main() {
    (void)words0;
    UNITY_BEGIN();
    RUN_TEST(test_roll_codes_pack_three_a_word);
    RUN_TEST(test_roll_write_cuts_and_cleans);
    RUN_TEST(test_roll_length_names);
    RUN_TEST(test_rol_default_riff_timing_and_lengths);
    RUN_TEST(test_rol_root_and_scale_transpose);
    RUN_TEST(test_rol_run_off_mutes_but_keeps_time);
    RUN_TEST(test_rol_transport_stop_and_restart);
    RUN_TEST(test_rol_external_clock_steps);
    RUN_TEST(test_rol_audition_plays_over);
    RUN_TEST(test_rol_swing_keeps_pairs);
    RUN_TEST(test_evo_same_seed_same_line_and_in_range);
    RUN_TEST(test_evo_mut_zero_freezes);
    RUN_TEST(test_evo_plays_in_scale_and_evolves);
    RUN_TEST(test_evo_start_replays_the_seed);
    RUN_TEST(test_seq_root_and_octave);
    RUN_TEST(test_clk_follows_transport);
    RUN_TEST(test_seq_restarts_with_the_transport);
    RUN_TEST(test_seq_restart_keeps_separate_gates);
    RUN_TEST(test_rol_restart_while_running_is_a_fresh_edge);
    RUN_TEST(test_rol_swing_odd_len_keeps_tempo);
    RUN_TEST(test_rol_tempo_change_takes_the_step_playing);
    return UNITY_END();
}
