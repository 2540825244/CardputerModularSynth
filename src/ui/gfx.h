#pragma once
#include <cstdint>

// What a picture draws with: palette indices (render/palette.h) on a 240 x 135 4-bit sprite. The
// device implements it over the M5Canvas (render/gfx_canvas.cpp); the native tests over a plain
// framebuffer that also catches draws outside the picture's box (test/test_pictures).
enum class Align : uint8_t { Left, Center, Right };

class Gfx {
   public:
    virtual ~Gfx() = default;
    virtual void pixel(int x, int y, uint8_t c) = 0;
    virtual void line(int x0, int y0, int x1, int y1, uint8_t c) = 0;
    virtual void hline(int x, int y, int w, uint8_t c) = 0;
    virtual void vline(int x, int y, int h, uint8_t c) = 0;
    virtual void fillRect(int x, int y, int w, int h, uint8_t c) = 0;
    virtual void rect(int x, int y, int w, int h, uint8_t c) = 0;
    virtual void circle(int x, int y, int r, uint8_t c) = 0;
    virtual void fillCircle(int x, int y, int r, uint8_t c) = 0;
    // TomThumb (3 x 5, 4 px a character), y = the top of the glyphs.
    virtual void text(const char* s, int x, int y, uint8_t c, Align a = Align::Left) = 0;
    static constexpr int kCharW = 4, kTextH = 5;
};
