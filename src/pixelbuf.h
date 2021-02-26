#ifndef pixelbuf_h
#define pixelbuf_h

#include <stdint.h>

#include "config.h"
#include "glyphs.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define PIXELBUFSIZE (CHAR_HEIGHT * WIDTH / CHAR_WIDTH)

typedef struct pixelbuf {
    uint8_t buf[TEXTROW_BUFSIZE];
} pixelbuf;

pixelbuf pixelbuf_new();
void pixelbuf_clear(pixelbuf *p);
void pixelbuf_draw_unicode_glyph(pixelbuf *p, uint32_t pt, uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif