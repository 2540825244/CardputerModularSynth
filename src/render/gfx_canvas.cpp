#include "gfx_canvas.h"
#include "render_internal.h"

namespace rd {
namespace {
struct CanvasGfx : Gfx {
    void pixel(int x, int y, uint8_t c) override { cv.drawPixel(x, y, c); }
    void line(int x0, int y0, int x1, int y1, uint8_t c) override { cv.drawLine(x0, y0, x1, y1, c); }
    void hline(int x, int y, int w, uint8_t c) override { cv.drawFastHLine(x, y, w, c); }
    void vline(int x, int y, int h, uint8_t c) override { cv.drawFastVLine(x, y, h, c); }
    void fillRect(int x, int y, int w, int h, uint8_t c) override { cv.fillRect(x, y, w, h, c); }
    void rect(int x, int y, int w, int h, uint8_t c) override { cv.drawRect(x, y, w, h, c); }
    void circle(int x, int y, int r, uint8_t c) override { cv.drawCircle(x, y, r, c); }
    void fillCircle(int x, int y, int r, uint8_t c) override { cv.fillCircle(x, y, r, c); }
    void text(const char* s, int x, int y, uint8_t c, Align a) override {
        cv.setFont(&fonts::TomThumb);
        cv.setTextColor(c);
        cv.setTextDatum(a == Align::Left ? textdatum_t::top_left
                                         : (a == Align::Center ? textdatum_t::top_center : textdatum_t::top_right));
        cv.drawString(s, x, y);
    }
};
}  // namespace

Gfx& canvasGfx() {
    static CanvasGfx g;
    return g;
}
}  // namespace rd
