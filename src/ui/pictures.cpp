#include "pictures.h"
#include <cstdio>
#include <cstring>
#include "layout.h"
#include "pictures_internal.h"

namespace pic {
namespace {

const Entry* find(const char* id) {
    static const struct {
        const Entry* e;
        const int* n;
    } kTables[] = {{kSourcePics, &kNumSourcePics}, {kModPics, &kNumModPics}, {kProcPics, &kNumProcPics},
                   {kSeqPics, &kNumSeqPics}};
    for (const auto& t : kTables)
        for (int i = 0; i < *t.n; i++)
            if (!strcmp(t.e[i].id, id)) return &t.e[i];
    return nullptr;
}

}  // namespace

void noteName(float semis, char* out, int len) {
    static const char* const kN[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const float midi = 60.f + clampf(semis, -100.f, 100.f);
    const int note = (int)lroundf(midi);
    int cents = (int)lroundf((midi - (float)note) * 100.f);
    if (cents > 49) cents = 49;
    snprintf(out, (size_t)len, "%s%d %+d", kN[((note % 12) + 12) % 12], note / 12 - 1, cents);
}

float basicWave(int kind, float t) {
    const float f = t - floorf(t);
    switch (kind) {
        case 0: return sinf(6.2831853f * f);
        case 1: return f < 0.5f ? 4.f * f - 1.f : 3.f - 4.f * f;
        case 2: return 1.f - 2.f * f;
        default: return f < 0.5f ? 1.f : -1.f;
    }
}

// GRD's ENG order (grd.cpp): the 16 original Plaits engines, then the 8 of Plaits 1.2. Meanings
// from the Plaits manual, checked against lib/plaits/plaits/dsp/engine*/ (what each engine does
// with parameters.harmonics / timbre / morph).
namespace {
struct GrdEngine {
    const char* name;
    const char* lab[3];
    const char* full[3];
};
const GrdEngine kGrd[24] = {
    {"VIRTUAL ANALOG", {"DET", "SQR", "SAW"}, {"DETUNE", "SQUARE SHAPE", "SAW SHAPE"}},
    {"WAVESHAPER", {"WAV", "FLD", "ASY"}, {"WAVEFORM", "FOLD", "ASYMMETRY"}},
    {"2-OP FM", {"RAT", "IDX", "FBK"}, {"RATIO", "INDEX", "FEEDBACK"}},
    {"GRAIN FORMANT", {"RAT", "FRM", "WID"}, {"FORMANT RATIO", "FORMANT", "WIDTH"}},
    {"ADDITIVE", {"BMP", "PEK", "SHP"}, {"BUMPS", "PEAK HARMONIC", "BUMP SHAPE"}},
    {"WAVETABLE", {"BNK", "ROW", "COL"}, {"BANK", "ROW", "COLUMN"}},
    {"CHORDS", {"CHD", "INV", "WAV"}, {"CHORD", "INVERSION", "WAVEFORM"}},
    {"SPEECH", {"BNK", "SPC", "SEG"}, {"BANK", "SPECIES", "SEGMENT"}},
    {"SWARM", {"SPR", "DEN", "DUR"}, {"PITCH SPREAD", "DENSITY", "GRAIN LENGTH"}},
    {"FILTERED NOISE", {"FLT", "CLK", "RES"}, {"FILTER TYPE", "CLOCK", "RESONANCE"}},
    {"PARTICLES", {"SPR", "DEN", "FLT"}, {"FREQ SPREAD", "DENSITY", "FILTER"}},
    {"STRING", {"INH", "BRI", "DEC"}, {"INHARMONICITY", "BRIGHTNESS", "DECAY"}},
    {"MODAL", {"MAT", "BRI", "DEC"}, {"MATERIAL", "BRIGHTNESS", "DECAY"}},
    {"BASS DRUM", {"ATK", "BRI", "DEC"}, {"ATTACK / DRIVE", "BRIGHTNESS", "DECAY"}},
    {"SNARE DRUM", {"SNP", "TON", "DEC"}, {"SNAPPY", "TONE", "DECAY"}},
    {"HI-HAT", {"NOI", "TON", "DEC"}, {"NOISINESS", "TONE", "DECAY"}},
    {"VA + FILTER", {"RES", "CUT", "WAV"}, {"RESONANCE", "CUTOFF", "WAVEFORM"}},
    {"PHASE DISTORT", {"FRQ", "AMT", "ASY"}, {"DISTORT FREQ", "AMOUNT", "ASYMMETRY"}},
    {"6-OP FM A", {"PAT", "LVL", "ENV"}, {"PATCH", "MOD LEVEL", "ENVELOPE"}},
    {"6-OP FM B", {"PAT", "LVL", "ENV"}, {"PATCH", "MOD LEVEL", "ENVELOPE"}},
    {"6-OP FM C", {"PAT", "LVL", "ENV"}, {"PATCH", "MOD LEVEL", "ENVELOPE"}},
    {"WAVE TERRAIN", {"TER", "RAD", "OFS"}, {"TERRAIN", "PATH RADIUS", "PATH OFFSET"}},
    {"STRING MACHINE", {"CHD", "FLT", "WAV"}, {"CHORD", "FILTER / CHORUS", "REGISTRATION"}},
    {"CHIPTUNE", {"CHD", "ARP", "PW"}, {"CHORD", "ARPEGGIO", "PULSE WIDTH"}},
};
const GrdEngine& grd(int e) { return kGrd[e < 0 ? 0 : (e > 23 ? 23 : e)]; }
}  // namespace

const char* grdKnobLabel(int engine, int knob) { return grd(engine).lab[knob < 0 ? 0 : (knob > 2 ? 2 : knob)]; }
const char* grdKnobName(int engine, int knob) { return grd(engine).full[knob < 0 ? 0 : (knob > 2 ? 2 : knob)]; }
const char* grdEngineName(int engine) { return grd(engine).name; }

}  // namespace pic

bool hasPicture(const ModuleTypeInfo& t) {
    for (int p = 0; p < modulePageCount(t); p++)
        if (modulePageDrawing(t, p) == Drawing::Picture) return true;
    return false;
}

void drawPicture(const PicCtx& c) {
    if (c.w < 4 || c.h < 4) return;
    const pic::Entry* e = pic::find(c.m.type->id);
    if (e && e->paint) e->paint(c);
}

bool drawTileGlyph(Gfx& g, const ModuleRec& m, int x, int y, int w, int h, uint8_t color) {
    const pic::Entry* e = pic::find(m.type->id);
    return e && e->glyph && e->glyph(g, m, {x, y, w, h}, color);
}

ModuleRec defaultRecord(const ModuleTypeInfo& t) {
    ModuleRec r{};
    r.type = &t;
    for (int i = 0; i < t.numParams && i < kMaxParams; i++) r.params[i] = t.params[i].def;
    for (auto& a : r.outAmount) a = 1.f;
    for (auto& a : r.inAmount) a = 1.f;
    return r;
}

namespace {
bool isGrdKnob(const ModuleRec& m, int idx) { return !strcmp(m.type->id, "grd") && idx >= 3 && idx <= 5; }
}  // namespace

const char* paramLabel(const ModuleRec& m, int idx) {
    if (isGrdKnob(m, idx)) return pic::grdKnobLabel((int)lroundf(m.params[0]), idx - 3);
    return m.type->params[idx].label;
}

const char* paramName(const ModuleRec& m, int idx, char* buf, int len) {
    const char* s = isGrdKnob(m, idx) ? pic::grdKnobName((int)lroundf(m.params[0]), idx - 3) : m.type->params[idx].key;
    int i = 0;
    for (; s[i] && i + 1 < len; i++) buf[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    buf[i] = 0;
    return buf;
}
