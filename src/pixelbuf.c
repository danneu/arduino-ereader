
#include "pixelbuf.h"

#include <math.h>

#include "util.h"

pixelbuf pixelbuf_new() { return (pixelbuf){{0xff}}; }

void pixelbuf_clear(pixelbuf *p) {
    for (uint16_t i = 0; i < TEXTROW_BUFSIZE; i++) {
        p->buf[i] = 0xff;
    }
}

// y is 0..CHAR_HEIGHT
void pixelbuf_draw_horiz(pixelbuf *p, uint8_t y) {
    for (uint8_t idx = 0; idx < CHARS_PER_ROW; idx++) {
        p->buf[WIDTH / 8 * y + idx] = 0x00;
    }
}

// ratio is 0.0 to 1.0
void pixelbuf_draw_progress(pixelbuf *p, double ratio) {
    double limit = ratio * WIDTH / 8;
    uint8_t y = 11;

    uint8_t partial;
    switch ((int)round((limit - (int)limit) * 8)) {
        case 0:
            partial = 0b11111111;
            break;
        case 1:
            partial = 0b01111111;
            break;
        case 2:
            partial = 0b00111111;
            break;
        case 3:
            partial = 0b00011111;
            break;
        case 4:
            partial = 0b00001111;
            break;
        case 5:
            partial = 0b00000111;
            break;
        case 6:
            partial = 0b00000011;
            break;
        case 7:
            partial = 0b00000001;
            break;
        case 8:
            partial = 0b00000000;
            break;
        default:
            partial = 0b11111111;
            break;
    }
    for (uint16_t x = 0; x < WIDTH / 8; x++) {
        if (x < (int)limit) {
            p->buf[WIDTH / 8 * y + x] = 0x00;
        } else if (x < ceil(limit)) {
            p->buf[WIDTH / 8 * y + x] = partial;
        } else {
            break;
        }
    }
}

void pixelbuf_draw_unicode_glyph(pixelbuf *p, uint32_t pt, uint8_t idx) {
    // FIXME: Crappy hack
    idx++;

    uint8_t glyph[16];
    int notfound = get_glyph(pt, glyph);
    if (notfound) {
        // get_glyph('_', glyph);  // For development
        get_glyph(' ', glyph);  // For production
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        p->buf[WIDTH / 8 * y + idx] = ~glyph[y];
    }
}