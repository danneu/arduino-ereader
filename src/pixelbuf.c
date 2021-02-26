
#include "pixelbuf.h"

pixelbuf pixelbuf_new() { return (pixelbuf){{0xff}}; }

void pixelbuf_clear(pixelbuf *p) {
    for (uint16_t i = 0; i < TEXTROW_BUFSIZE; i++) {
        p->buf[i] = 0xff;
    }
}

void pixelbuf_draw_unicode_glyph(pixelbuf *p, uint32_t pt, uint8_t idx) {
    // FIXME: Crappy hack
    idx++;

    uint8_t glyph[16];
    int notfound = get_glyph(pt, glyph);
    if (notfound) {
        get_glyph('_', glyph);  // For development
        // get_glyph(' ', glyph); // For production
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        p->buf[WIDTH / 8 * y + idx] = ~glyph[y];
    }
}