#include <unity.h>
#include "old_grd.h"
#include "stmlib/utils/random.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr int kIns = 9;

struct Run {
    std::vector<float> out[4];
};

// Inputs: a gate on TRG every ~0.3 s, a slow pitch ramp, CUT and LVL wobbling.
struct In {
    std::vector<float> ch[kIns];
    bool conn[kIns] = {};
};

In makeInputs(int blocks, bool cut, bool level) {
    In in;
    const int n = blocks * kBlock;
    in.ch[1].resize(n);  // TRG
    in.ch[0].resize(n);  // PIT
    for (int i = 0; i < n; i++) {
        in.ch[1][i] = (i % 14000) < 5000 ? 5.f : 0.f;
        in.ch[0][i] = -1.f + 2.f * (float)i / (float)n;
    }
    in.conn[0] = in.conn[1] = true;
    if (cut) {
        in.ch[8].resize(n);
        for (int i = 0; i < n; i++) in.ch[8][i] = 2.f * sinf((float)i * 0.0007f);
        in.conn[8] = true;
    }
    if (level) {
        in.ch[6].resize(n);
        for (int i = 0; i < n; i++) in.ch[6][i] = 2.5f + 2.5f * sinf((float)i * 0.0003f);
        in.conn[6] = true;
    }
    return in;
}

Run run(Module* m, const In& in, int blocks, const float* params) {
    for (int i = 0; i < kGrdType.numParams; i++) m->setParam(i, params[i]);
    Run r;
    for (int b = 0; b < blocks; b++) {
        const float* ins[kMaxIns] = {};
        bool conn[kMaxIns] = {};
        for (int k = 0; k < kIns; k++) {
            conn[k] = in.conn[k];
            ins[k] = in.conn[k] ? in.ch[k].data() + b * kBlock : nullptr;
        }
        m->process(ins, conn);
        for (int o = 0; o < 4; o++) r.out[o].insert(r.out[o].end(), m->out(o), m->out(o) + kBlock);
    }
    return r;
}

void check(int eng, int flt, int vca, bool cut, bool level) {
    float params[kMaxParams];
    for (int i = 0; i < kGrdType.numParams; i++) params[i] = kGrdType.params[i].def;
    params[oldgrd::kEng] = (float)eng;
    params[oldgrd::kFlt] = (float)flt;
    params[oldgrd::kVca] = (float)vca;
    params[oldgrd::kRes] = 0.6f;
    params[oldgrd::kLCut] = 0.4f;
    params[oldgrd::kLTim] = 0.3f;
    const int blocks = 900;  // 0.6 s
    const In in = makeInputs(blocks, cut, level);
    Module* now = kGrdType.create();
    char* shared = new char[oldgrd::kShared];
    Module* old = new oldgrd::OldGrd(shared);
    // stmlib's Random is one static state: start both runs from the same seed
    stmlib::Random::Seed(1);
    const Run a = run(now, in, blocks, params);
    stmlib::Random::Seed(1);
    const Run b = run(old, in, blocks, params);
    delete now;
    delete old;
    float peak = 0.f;
    for (int o = 0; o < 4; o++)
        for (size_t i = 0; i < a.out[o].size(); i++) {
            peak = fmaxf(peak, fabsf(b.out[o][i]));
            if (a.out[o][i] != b.out[o][i]) {
                char m[128];
                snprintf(m, sizeof m, "eng %d flt %d vca %d cut %d lvl %d: out %d sample %u: %g vs %g", eng, flt,
                         vca, cut, level, o, (unsigned)i, (double)a.out[o][i], (double)b.out[o][i]);
                TEST_FAIL_MESSAGE(m);
            }
        }
    TEST_ASSERT_TRUE(peak > 0.1f);  // not silent
}

}  // namespace

// Bit-identical: the passes do the same float operations in the same order.
void test_grd_matches_old_ladder_lp() {
    for (int eng : {0, 3, 11, 16}) check(eng, 0, 0, false, false);
}
void test_grd_matches_old_ladder_hp_cut_level() {
    for (int eng : {0, 5}) check(eng, 1, 0, true, true);
}
void test_grd_matches_old_filter_off_lpg_drone() {
    check(0, 2, 1, false, true);
    check(13, 2, 2, true, false);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_grd_matches_old_ladder_lp);
    RUN_TEST(test_grd_matches_old_ladder_hp_cut_level);
    RUN_TEST(test_grd_matches_old_filter_off_lpg_drone);
    return UNITY_END();
}
