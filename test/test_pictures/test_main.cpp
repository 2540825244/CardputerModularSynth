// v1.4 module pictures: every painter, at every size, with params at min / default / max, stays
// inside its box, draws something and never NaNs; the picture maths; and a PNG of each picture
// in .pio/pictures/ to look at without the device.
#include <unity.h>
#include <sys/stat.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "core/registry.h"
#include "render/palette.h"
#include "ui/layout.h"
#include "ui/pictures.h"

// ---- TomThumb from M5GFX (BSD), for real glyphs in the PNGs ----
#if __has_include("../../.pio/libdeps/cardputer/M5GFX/src/lgfx/Fonts/GFXFF/TomThumb.h")
#define PROGMEM
struct GFXglyph {
    uint16_t bitmapOffset;
    uint8_t width, height, xAdvance;
    int8_t xOffset, yOffset;
};
struct GFXfont {
    uint8_t* bitmap;
    GFXglyph* glyph;
    uint16_t first, last;
    uint8_t yAdvance;
};
#include "../../.pio/libdeps/cardputer/M5GFX/src/lgfx/Fonts/GFXFF/TomThumb.h"
#define HAVE_FONT 1
#endif

namespace {

constexpr int kW = 240, kH = 135;

struct FbGfx : Gfx {
    uint8_t px[kH][kW];
    int bx = 0, by = 0, bw = kW, bh = kH;  // the box a picture must stay in
    int outside = 0, drawn = 0, nan = 0;
    FbGfx() { clear(PEmpty); }
    void clear(uint8_t c) { memset(px, c, sizeof px); outside = drawn = nan = 0; }
    void box(int x, int y, int w, int h) { bx = x; by = y; bw = w; bh = h; }
    void pixel(int x, int y, uint8_t c) override {
        if (x < bx || y < by || x >= bx + bw || y >= by + bh) {
            outside++;
            return;
        }
        if (x >= 0 && y >= 0 && x < kW && y < kH) {
            px[y][x] = c;
            drawn++;
        }
    }
    void line(int x0, int y0, int x1, int y1, uint8_t c) override {
        if (std::abs(x0) > 100000 || std::abs(y0) > 100000 || std::abs(x1) > 100000 || std::abs(y1) > 100000) {
            nan++;
            return;
        }
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int e = dx + dy;
        for (;;) {
            pixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * e;
            if (e2 >= dy) { e += dy; x0 += sx; }
            if (e2 <= dx) { e += dx; y0 += sy; }
        }
    }
    void hline(int x, int y, int w, uint8_t c) override { for (int i = 0; i < w; i++) pixel(x + i, y, c); }
    void vline(int x, int y, int h, uint8_t c) override { for (int i = 0; i < h; i++) pixel(x, y + i, c); }
    void fillRect(int x, int y, int w, int h, uint8_t c) override { for (int j = 0; j < h; j++) hline(x, y + j, w, c); }
    void rect(int x, int y, int w, int h, uint8_t c) override {
        if (w <= 0 || h <= 0) return;
        hline(x, y, w, c); hline(x, y + h - 1, w, c); vline(x, y, h, c); vline(x + w - 1, y, h, c);
    }
    void circle(int x0, int y0, int r, uint8_t c) override {
        for (int a = 0; a < 64; a++) {
            const float t = a * 6.2831853f / 64.f;
            pixel(x0 + (int)lroundf(r * cosf(t)), y0 + (int)lroundf(r * sinf(t)), c);
        }
    }
    void fillCircle(int x0, int y0, int r, uint8_t c) override {
        for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r) pixel(x0 + x, y0 + y, c);
    }
    void text(const char* s, int x, int y, uint8_t c, Align a) override {
        const int w = (int)strlen(s) * kCharW;
        if (a == Align::Center) x -= w / 2;
        else if (a == Align::Right) x -= w;
#ifdef HAVE_FONT
        for (; *s; s++, x += kCharW) {
            const int ch = (unsigned char)*s;
            if (ch < 0x20 || ch > 0x7E) continue;
            const GFXglyph& gl = TomThumbGlyphs[ch - 0x20];
            int bit = 0;
            for (int r = 0; r < gl.height; r++)
                for (int k = 0; k < gl.width; k++, bit++)
                    if (TomThumbBitmaps[gl.bitmapOffset + bit / 8] & (0x80 >> (bit % 8)))
                        pixel(x + gl.xOffset + k, y + 5 + gl.yOffset + r, c);
        }
#else
        fillRect(x, y, w - 1, kTextH, c);
#endif
    }
};

// ---- PNG, stored deflate blocks (no zlib) ----
uint32_t crc32(const uint8_t* d, size_t n, uint32_t c = 0) {
    c = ~c;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}
void be32(std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s)); }
void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, (uint32_t)data.size());
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    out.insert(out.end(), td.begin(), td.end());
    be32(out, crc32(td.data(), td.size()));
}
// pix(x, y) -> palette index
template <class Pix>
void writePngOf(const char* path, Pix pix, int w, int h, int scale, int theme) {
    const uint32_t* pal = theme ? kStyleA : kStyleB;
    const int W = w * scale, H = h * scale;
    std::vector<uint8_t> raw;
    for (int y = 0; y < H; y++) {
        raw.push_back(0);
        for (int x = 0; x < W; x++) {
            const uint32_t c = pal[pix(x / scale, y / scale) & 15];
            raw.push_back((uint8_t)(c >> 16)); raw.push_back((uint8_t)(c >> 8)); raw.push_back((uint8_t)c);
        }
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t i = 0; i < raw.size(); i += 65535) {
        const size_t n = raw.size() - i < 65535 ? raw.size() - i : 65535;
        z.push_back(i + n == raw.size() ? 1 : 0);
        z.push_back((uint8_t)n); z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)~n); z.push_back((uint8_t)(~n >> 8));
        z.insert(z.end(), raw.begin() + (long)i, raw.begin() + (long)(i + n));
    }
    be32(z, (b << 16) | a);
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    be32(ihdr, (uint32_t)W); be32(ihdr, (uint32_t)H);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    FILE* fp = fopen(path, "wb");
    if (!fp) return;
    fwrite(png.data(), 1, png.size(), fp);
    fclose(fp);
}
void writePng(const char* path, const FbGfx& f, int x0, int y0, int w, int h, int scale, int theme) {
    writePngOf(path, [&](int x, int y) { return f.px[y0 + y][x0 + x]; }, w, h, scale, theme);
}

// The module-view picture block (render/draw_module.cpp): cols 5..11, rows 1..3.
constexpr int kBigX = 102, kBigY = 33;
constexpr int kBigW = 121, kBigH = 101;  // cellX(11) + cellW(11) - cellX(5), cellY(3) + cellH(3) - cellY(1)

std::vector<const ModuleTypeInfo*> pictured() {
    int n = 0;
    const ModuleTypeInfo* const* all = allTypes(&n);
    std::vector<const ModuleTypeInfo*> r;
    for (int i = 0; i < n; i++) if (hasPicture(*all[i])) r.push_back(all[i]);
    return r;
}

std::vector<int> picturePages(const ModuleTypeInfo& t) {
    std::vector<int> r;
    for (int p = 0; p < modulePageCount(t); p++) if (modulePageDrawing(t, p) == Drawing::Picture) r.push_back(p);
    return r;
}

void paint(FbGfx& f, const ModuleRec& m, int page, int sel, int x, int y, int w, int h, bool small) {
    f.box(x, y, w, h);
    drawPicture(PicCtx{f, m, nullptr, page, sel, x, y, w, h, small});
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_block_matches_the_grid() {
    // the constants above are the renderer's box: cols 5..11, rows 1..3 of the 12 x 4 grid
    TEST_ASSERT_EQUAL(kBigX, 16 + 5 * 208 / 12);
    TEST_ASSERT_EQUAL(kBigY, 1 * 135 / 4);
    TEST_ASSERT_EQUAL(kBigW, (16 + 12 * 208 / 12 - 1) - kBigX);
    TEST_ASSERT_EQUAL(kBigH, (135 - 1) - kBigY);
}

void test_picture_pages_match_painters() {
    int n = 0;
    const ModuleTypeInfo* const* all = allTypes(&n);
    for (int i = 0; i < n; i++) {
        const bool pages = !picturePages(*all[i]).empty();
        TEST_ASSERT_EQUAL_MESSAGE(pages, hasPicture(*all[i]), all[i]->id);
    }
}

void test_pictures_stay_in_their_box() {
    FbGfx f;
    for (const ModuleTypeInfo* t : pictured()) {
        for (int page : picturePages(*t)) {
            for (int variant = 0; variant < 3; variant++) {
                ModuleRec m = defaultRecord(*t);
                for (int i = 0; i < t->numParams; i++)
                    if (variant == 1) m.params[i] = t->params[i].min;
                    else if (variant == 2) m.params[i] = t->params[i].max;
                for (int sel = -1; sel < (int)t->numParams; sel++) {
                    f.clear(PEmpty);
                    paint(f, m, page, sel, kBigX, kBigY, kBigW, kBigH, false);
                    char msg[64];
                    snprintf(msg, sizeof msg, "%s page %d variant %d sel %d", t->id, page, variant, sel);
                    TEST_ASSERT_EQUAL_MESSAGE(0, f.outside, msg);
                    TEST_ASSERT_EQUAL_MESSAGE(0, f.nan, msg);
                    TEST_ASSERT_TRUE_MESSAGE(f.drawn > 40, msg);
                }
                f.clear(PEmpty);
                paint(f, m, page, -1, 10, 10, 40, 26, true);  // a thumbnail
                TEST_ASSERT_EQUAL_MESSAGE(0, f.outside, t->id);
                TEST_ASSERT_EQUAL_MESSAGE(0, f.nan, t->id);
            }
        }
    }
}

void test_tile_glyphs_stay_in_their_box() {
    FbGfx f;
    int n = 0;
    const ModuleTypeInfo* const* all = allTypes(&n);
    for (int i = 0; i < n; i++) {
        for (int variant = 0; variant < 3; variant++) {
            ModuleRec m = defaultRecord(*all[i]);
            for (int k = 0; k < all[i]->numParams; k++)
                if (variant == 1) m.params[k] = all[i]->params[k].min;
                else if (variant == 2) m.params[k] = all[i]->params[k].max;
            f.clear(PEmpty);
            f.box(20, 20, 13, 11);
            drawTileGlyph(f, m, 20, 20, 13, 11, PText);
            TEST_ASSERT_EQUAL_MESSAGE(0, f.outside, all[i]->id);
            TEST_ASSERT_EQUAL_MESSAGE(0, f.nan, all[i]->id);
        }
    }
}

void test_note_name() {
    char s[16];
    pic::noteName(0.f, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("C4 +0", s);
    pic::noteName(9.f, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("A4 +0", s);
    pic::noteName(-12.f + 0.12f, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("C3 +12", s);
    pic::noteName(-0.3f, s, sizeof s);  // 30 cents flat of C4
    TEST_ASSERT_EQUAL_STRING("C4 -30", s);
    pic::noteName(0.6f, s, sizeof s);   // 60 cents sharp: C#4 -40
    TEST_ASSERT_EQUAL_STRING("C#4 -40", s);
}

void test_basic_waves() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.f, pic::basicWave(0, 0.f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, pic::basicWave(0, 0.25f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, pic::basicWave(1, 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.f, pic::basicWave(1, 0.f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, pic::basicWave(2, 0.f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, pic::basicWave(3, 0.1f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.f, pic::basicWave(3, 0.6f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, pic::basicWave(2, 1.0f));  // wraps
}

// Writes every picture (theme B, 3x) to .pio/pictures/<id>[-p<page>].png, plus a contact sheet.
void test_dump_pictures() {
    mkdir(".pio/pictures", 0755);
    FbGfx f;
    // contact sheet: every big picture (selection on param 0), 5 a row, with its id
    constexpr int kTW = kBigW + 4, kTH = kBigH + 10, kCols = 5;
    std::vector<std::pair<const ModuleTypeInfo*, int>> all;
    for (const ModuleTypeInfo* t : pictured()) for (int page : picturePages(*t)) all.push_back({t, page});
    const int rows = ((int)all.size() + kCols - 1) / kCols;
    std::vector<uint8_t> sheet((size_t)(kTW * kCols * kTH * rows), PStrip);
    for (size_t k = 0; k < all.size(); k++) {
        f.clear(PEmpty);
        const ModuleRec m = defaultRecord(*all[k].first);
        paint(f, m, all[k].second, -1, kBigX, kBigY, kBigW, kBigH, false);
        f.box(0, 0, kW, kH);
        f.fillRect(0, 0, kW, 9, PStrip);
        char id[24];
        snprintf(id, sizeof id, "%s %d", all[k].first->id, all[k].second);
        f.text(id, kBigX, 2, PText, Align::Left);
        const int ox = (int)(k % kCols) * kTW, oy = (int)(k / kCols) * kTH;
        for (int y = 0; y < kBigH + 9; y++)
            for (int x = 0; x < kBigW; x++)
                sheet[(size_t)((oy + y) * kTW * kCols + ox + x)] = y < 9 ? f.px[y][kBigX + x] : f.px[kBigY + y - 9][kBigX + x];
    }
    writePngOf(".pio/pictures/_sheet.png", [&](int x, int y) { return sheet[(size_t)(y * kTW * kCols + x)]; }, kTW * kCols,
               kTH * rows, 2, 0);
    for (const ModuleTypeInfo* t : pictured()) {
        for (int page : picturePages(*t)) {
            const ModuleRec m = defaultRecord(*t);
            f.clear(PEmpty);
            paint(f, m, page, 0, kBigX, kBigY, kBigW, kBigH, false);
            f.box(0, 0, kW, kH);
            f.fillRect(40, 4, 44, 28, PBg);
            paint(f, m, page, -1, 42, 5, 40, 26, true);
            f.box(0, 0, kW, kH);
            f.fillRect(88, 8, 15, 13, PBg);
            f.box(89, 9, 13, 11);
            drawTileGlyph(f, m, 89, 9, 13, 11, PText);
            char path[96];
            if (page) snprintf(path, sizeof path, ".pio/pictures/%s-p%d.png", t->id, page);
            else snprintf(path, sizeof path, ".pio/pictures/%s.png", t->id);
            writePng(path, f, 38, 2, 186, 133, 3, 0);
        }
    }
    TEST_PASS();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_block_matches_the_grid);
    RUN_TEST(test_picture_pages_match_painters);
    RUN_TEST(test_pictures_stay_in_their_box);
    RUN_TEST(test_tile_glyphs_stay_in_their_box);
    RUN_TEST(test_note_name);
    RUN_TEST(test_basic_waves);
    RUN_TEST(test_dump_pictures);
    return UNITY_END();
}
