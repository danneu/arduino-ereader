// Small abstraction around the Petit Fat library for
// loading from SD card

#ifndef book_h
#define book_h

#include <diskio.h>
#include <pff.h>
#include <stdint.h>

#include "pixelbuf.h"
#include "utf8.h"

#ifdef __cplusplus
extern "C" {
#endif

// Result of trying to get the next unicode codepoint from our byte offset
// location in the book in the book.
typedef struct PTRESULT {
    // UTF8 decode result: OK | INVALID | INCOMPLETE
    UTF8Status evt;
    // If we're at the end of the book.
    bool eob;
    // Decoded codepoint
    uint32_t pt;
    // Byte width of decoded unicode glyph
    uint8_t width;
} PTRESULT;

typedef struct State {
    FATFS *fs;
    // char fname[];
    uint32_t fsize;
    // TODO: CLear buffer when skipping aroun book.
    uint8_t buf[64];
    uint8_t bufidx;
    UINT buflen;

    // Page offset history
    uint32_t history[16];
    int8_t hid;
} Struct;

State new_state(FATFS *fs, uint32_t fsize);
PTRESULT next_codepoint(State *s);
// uint16_t show_offset(State *s, uint32_t offset, pixelbuf *frame);
uint32_t next_page(State *s, pixelbuf *frame);
void prev_page(State *s, pixelbuf *frame);
void abort_with_ferror(FRESULT res, pixelbuf *frame);

#ifdef __cplusplus
}
#endif

#endif