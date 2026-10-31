// v1.5 pictures: ROL's piano roll (its SET-page picture and the whole ROLL page) and EVO's line.
#include <cstring>
#include "core/modules/seq2.h"
#include "core/scales.h"
#include "core/transport.h"
#include "pictures_internal.h"
#include "views.h"

namespace pic {
namespace {

// "C3" for semitones above ROOT at OCT.
void noteAt(const ModuleRec& m, int semis, char* out, int len) {
    const int midi = 60 + (int)m.params[roll::kRoot] + 12 * (int)m.params[roll::kOct] + semis;
    snprintf(out, (size_t)len, "%s%d", kNoteNames[((midi % 12) + 12) % 12], (midi >= 0 ? midi / 12 : (midi - 11) / 12) - 1);
}
// What a stored note plays: snapped to the scale, as the module does.
int heard(const ModuleRec& m, int code) {
    return (int)lroundf(quantize((float)(code - roll::kBase) / 12.f, (int)m.params[roll::kScale], 0) * 12.f);
}

struct RollMarks {
    int cursor = -1, curLen = 1;  // the roll page's cursor and the chosen length
    int play = -1;                // the step playing (live), -1 none
    bool guide = false;           // a pitch key is held: draw its row
    int guideSemis = 0;
};

// The roll: steps across, pitch up. Notes are bars as long as they last; beats and bars are
// dashed / solid lines; ROOT rows are dashed and named in the gutter (big only).
void drawRoll(Gfx& g, const ModuleRec& m, Box b, const RollMarks& k, bool small) {
    using namespace roll;
    const float* w = m.params + kN0;
    int len = (int)m.params[kLen];
    len = len < 1 ? 1 : (len > kSteps ? kSteps : len);
    // the pitch window: every note (and the guide), padded, at least an octave and a bit
    int lo = 1000, hi = -1000;
    for (int s = 0; s < len; s++) {
        const int c = code(w, s);
        if (!isNote(c)) continue;
        const int p = heard(m, c);
        lo = p < lo ? p : lo;
        hi = p > hi ? p : hi;
    }
    if (k.guide) {
        lo = k.guideSemis < lo ? k.guideSemis : lo;
        hi = k.guideSemis > hi ? k.guideSemis : hi;
    }
    if (lo > hi) lo = hi = 0;
    lo -= 2;
    hi += 2;
    while (hi - lo < 14) {
        lo--;
        hi++;
    }
    const int gutter = small ? 0 : 13;
    const Box r = {b.x + gutter, b.y, b.w - gutter, b.h};
    int rowH = r.h / (hi - lo + 1);
    if (rowH < 2) {  // too wide a range: centre what fits
        rowH = 2;
        const int fit = r.h / 2, mid = (lo + hi) / 2;
        lo = mid - fit / 2;
        hi = lo + fit - 1;
    }
    const int top = r.y + (r.h - rowH * (hi - lo + 1)) / 2;
    auto yOf = [&](int semis) { return top + (hi - semis) * rowH; };
    auto xOf = [&](int step) { return r.x + step * r.w / len; };
    const int spb = stepsPerBeat((int)m.params[kRes]);
    if (!small) {
        for (int s = spb; s < len; s += spb) {
            if (s % (4 * spb) == 0) g.vline(xOf(s), r.y, r.h, PDim);
            else dashV(g, xOf(s), r.y, r.h, PBorder, 1, 2);
        }
        for (int p = hi; p >= lo; p--) {
            if (((p % 12) + 12) % 12 != 0) continue;
            dashH(g, r.x, yOf(p) + rowH / 2, r.w, PBorder, 1, 3);
            char n[8];
            noteAt(m, p, n, sizeof n);
            g.text(n, b.x, yOf(p) + rowH / 2 - 2, PDim);
        }
        if (k.guide && k.guideSemis >= lo && k.guideSemis <= hi) g.fillRect(r.x, yOf(k.guideSemis), r.w, rowH, PFaint);
        if (k.play >= 0 && k.play < len) g.fillRect(xOf(k.play), r.y, xOf(k.play + 1) - xOf(k.play), 2, PText);
    }
    const int ps = k.play >= 0 ? noteStart(w, k.play < len ? k.play : len - 1) : -1;
    for (int s = 0; s < len; s++) {
        const int c = code(w, s);
        if (!isNote(c)) continue;
        const int n = noteLength(w, s, len), p = heard(m, c);
        if (p < lo || p > hi) continue;
        const int x0 = xOf(s), x1 = xOf(s + n);
        const bool atCursor = k.cursor >= s && k.cursor < s + n;
        const uint8_t col = s == ps || atCursor ? PAccent : PText;
        const int bw = x1 - x0 - 1 > 1 ? x1 - x0 - 1 : 1, bh = rowH > 2 ? rowH - 1 : rowH;
        g.fillRect(x0 + (bw > 2 ? 1 : 0), yOf(p), bw > 2 ? bw - 1 : bw, bh, col);
        if (!small && bw > 4 && n > 1) g.vline(x0 + 1, yOf(p) - 1, bh + 2, col);  // a note's head
    }
    if (k.cursor >= 0 && k.cursor < len) {  // where the next note goes, as long as it will be
        const int x0 = xOf(k.cursor), x1 = xOf(k.cursor + k.curLen < len ? k.cursor + k.curLen : len);
        g.rect(x0, r.y, x1 - x0 + 1 > 2 ? x1 - x0 + 1 : 2, r.h, PAccent);
        g.fillRect(x0, r.y + r.h - 3, x1 - x0 + 1 > 2 ? x1 - x0 + 1 : 2, 3, PAccent);
    }
}

// ---- ROL (SET page): the pattern, with ROOT / SCL / OCT and LEN / RES ----
void rol(const PicCtx& c) {
    using namespace roll;
    RollMarks k;
    if (live(c)) k.play = (int)mon(c, 0, -1.f);
    drawRoll(c.g, c.m, c.small ? body(c) : insetXY(body(c), 0, 1), k, c.small);
    if (c.small) return;
    char l[24], r[24], lab[8];
    snprintf(l, sizeof l, "%s %s OCT%+d", kNoteNames[ipar(c, kRoot) % 12], enumName(c, kScale), ipar(c, kOct));
    lengthName(1, ipar(c, kRes), lab, sizeof lab);
    snprintf(r, sizeof r, "%d X %s", ipar(c, kLen), lab);
    captions(c, l, c.sel == kRoot || c.sel == kScale || c.sel == kOct ? PAccent : PText, r,
             c.sel == kLen || c.sel == kRes ? PAccent : PDim);
}
bool rolGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    using namespace roll;
    const float* w = m.params + kN0;
    int lo = 1000, hi = -1000;
    for (int s = 0; s < 16; s++)
        if (isNote(code(w, s))) {
            lo = code(w, s) < lo ? code(w, s) : lo;
            hi = code(w, s) > hi ? code(w, s) : hi;
        }
    if (lo > hi) return false;
    for (int s = 0; s < 16; s++) {
        const int c = code(w, s);
        if (!isNote(c)) continue;
        const int n = noteLength(w, s, 16);
        const int y = b.y + (hi == lo ? b.h / 2 : (hi - c) * (b.h - 2) / (hi - lo));
        const int x0 = b.x + s * b.w / 16, x1 = b.x + (s + n) * b.w / 16;
        g.hline(x0, y, x1 - x0 > 1 ? x1 - x0 - 1 : 1, col);
        g.hline(x0, y + 1, x1 - x0 > 1 ? x1 - x0 - 1 : 1, col);
    }
    return true;
}

// ---- EVO: the line now (live; the seed's line when idle): notes at their degree, ties joined,
// accents filled, rests as dots; the step playing; the generation ----
void evoPic(const PicCtx& c) {
    using namespace evo;
    Line line;
    if (live(c)) {
        static_cast<const EvoModule*>(c.inst)->line(line);
    } else {
        mods::Rng rng;
        seedLine((uint32_t)ipar(c, kSeed), par(c, kDens), ipar(c, kRange), line, rng);
    }
    int len = ipar(c, kLen);
    len = len < 1 ? 1 : (len > kMaxSteps ? kMaxSteps : len);
    const int range = ipar(c, kRange) < 1 ? 1 : ipar(c, kRange);
    const Box b = c.small ? body(c) : band(body(c), 0.f, 0.86f);
    const int now = live(c) ? (int)mon(c, 0, -1.f) : -1;
    const uint8_t ink = c.sel < 0 || c.sel == kMut || c.sel == kDens || c.sel == kRange || c.sel == kSeed ? PAccent : PText;
    if (!c.small) dashH(c.g, b.x, ySigned(b, 0.f), b.w, PBorder, 1, 3);  // ROOT
    auto yOf = [&](int d) { return ySigned(insetXY(b, 0, 2), (float)(d < -range ? -range : (d > range ? range : d)) / (float)range); };
    int prevY = -1;
    for (int i = 0; i < len; i++) {
        const int x0 = b.x + i * b.w / len, x1 = b.x + (i + 1) * b.w / len;
        const int bw = x1 - x0 - 1 > 1 ? x1 - x0 - 1 : 1;
        if (!c.small && i == now) c.g.fillRect(x0, b.y, bw, b.h, PFaint);
        const uint8_t f = line.flags[i];
        if (!(f & kOn)) {
            c.g.pixel(x0 + bw / 2, b.y + b.h - 2, PDim);
            prevY = -1;
            continue;
        }
        const bool tie = (f & kTieF) && prevY >= 0;
        const int y = tie ? prevY : yOf(line.deg[i]);
        const int h = c.small ? 2 : 4;
        if (f & kAcc || c.small) c.g.fillRect(x0, y - h / 2, bw, h, i == now ? PText : ink);
        else c.g.rect(x0, y - h / 2, bw, h, i == now ? PText : ink);
        if (tie) c.g.hline(x0 - 1, y, 2, ink);
        prevY = y;
    }
    if (c.small) return;
    char l[24], r[24], bl[32];
    if (live(c)) snprintf(l, sizeof l, "GEN %u", (unsigned)static_cast<const EvoModule*>(c.inst)->generation());
    else snprintf(l, sizeof l, "SEED %d", ipar(c, kSeed));
    snprintf(r, sizeof r, "%s %s", kNoteNames[ipar(c, kRoot) % 12], enumName(c, kScale));
    captions(c, l, isSel(c, kSeed) ? PAccent : PText, r, c.sel == kRoot || c.sel == kScale ? PAccent : PDim);
    snprintf(bl, sizeof bl, "MUT %d%%  DNS %d%%", (int)lroundf(par(c, kMut) * 100.f), (int)lroundf(par(c, kDens) * 100.f));
    label(c, bl, c.x + 2, c.y + c.h - 7, c.sel == kMut || c.sel == kDens ? PAccent : PDim);
}
bool evoGlyph(Gfx& g, const ModuleRec& m, Box b, uint8_t col) {
    using namespace evo;
    Line line;
    mods::Rng rng;
    const int range = (int)m.params[kRange] < 1 ? 1 : (int)m.params[kRange];
    seedLine((uint32_t)m.params[kSeed], m.params[kDens], range, line, rng);
    for (int i = 0; i < 8; i++) {
        if (!(line.flags[i] & kOn)) continue;
        const int x = b.x + i * b.w / 8, y = ySigned(b, (float)line.deg[i] / (float)range);
        g.fillRect(x, y, 2, 2, col);
    }
    return true;
}

}  // namespace

const Entry kRollPics[] = {{"rol", rol, rolGlyph}, {"evo", evoPic, evoGlyph}};
const int kNumRollPics = sizeof kRollPics / sizeof kRollPics[0];

}  // namespace pic

// The ROLL page over the whole grid (x, y, w, h): the number row's functions on row 0, the roll on
// rows 1..3 with the cursor and the playing step, a status line on top of it.
void drawRollPage(Gfx& g, const ModuleRec& m, const Module* inst, const UiState& s, int x, int y, int w, int h) {
    using namespace roll;
    using namespace pic;
    auto cx = [&](int col) { return x + col * w / kGridCols; };
    auto cw = [&](int col) { return cx(col + 1) - cx(col) - 1; };
    const int ch = h / kGridRows - 1, res = (int)m.params[kRes];
    static const char* const kSide[] = {"REST", "TIE", "OCT-", "OCT+"};
    for (int col = 0; col < kGridCols; col++) {
        const int x0 = cx(col), wc = cw(col);
        char lab[8];
        if (col < 8) lengthName(rollLenSteps(col), res, lab, sizeof lab);
        else snprintf(lab, sizeof lab, "%s", kSide[col - 8]);
        const bool on = col == s.rollLen;
        if (on) g.fillRect(x0, y, wc, ch, PAccent);
        else g.rect(x0, y, wc, ch, PBorder);
        g.text(lab, x0 + wc / 2, y + 4, on ? PBg : PText, Align::Center);
        if (col < 8) {  // the length as a bar
            const int bw = 1 + rollLenSteps(col) * (wc - 5) / 16;
            g.fillRect(x0 + 2, y + ch - 9, bw, 3, on ? PBg : PDim);
        } else if (col >= 10) {
            char k[8];
            snprintf(k, sizeof k, "%+d", (int)m.params[kKbd]);
            g.text(k, x0 + wc / 2, y + ch - 10, PDim, Align::Center);
        }
    }
    const int y1 = y + h / kGridRows;
    int len = (int)m.params[kLen];
    len = len < 1 ? 1 : (len > kSteps ? kSteps : len);
    const int cur = s.rollCursor < len ? s.rollCursor : 0;
    // status: the cursor and the note under it; the transport / RUN / tempo
    char l[40], r[24], n[8] = "-", v[8] = "";
    const float* words = m.params + kN0;
    const int st = noteStart(words, cur);
    if (st >= 0) {
        noteAt(m, heard(m, code(words, st)), n, sizeof n);
        lengthName(noteLength(words, st, len), res, v, sizeof v);
    }
    snprintf(l, sizeof l, "STEP %d/%d  %s %s", cur + 1, len, n, v);
    if (!transport::running()) snprintf(r, sizeof r, "STOPPED");
    else if (m.params[kRun] < 0.5f) snprintf(r, sizeof r, "RUN OFF");
    else snprintf(r, sizeof r, "%d BPM", (int)lroundf(m.params[kBpm]));
    g.text(l, x + 2, y1 + 1, PText);
    g.text(r, x + w - 2, y1 + 1, transport::running() && m.params[kRun] >= 0.5f ? PAccent : PDim, Align::Right);
    RollMarks k;
    k.cursor = cur;
    k.curLen = rollLenSteps(s.rollLen);
    k.play = inst && transport::running() ? (int)inst->monitor(0) : -1;
    k.guide = s.keysHeld != 0;
    k.guideSemis = s.rollSemis;
    drawRoll(g, m, Box{x + 1, y1 + 8, w - 2, y + h - y1 - 9}, k, false);
}
