#ifndef utf8_h
#define utf8_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum UTF8Status {
    // 0: Successful decode. `width` tells how many bytes it took
    // to decode the codepoint.
    UTF8_OK = 0,
    // 1: Invalid utf8 byte sequence. `width` will be the length
    // of it, i.e. the amount of bytes to skip to begin decoding
    // the next codepoint.
    UTF8_INVALID = 1,
    // 2: Ran out of input. `width` will be
    // the length of valid sequence we consumed so far when we hit EOI.
    UTF8_EOI = 2,
} UTF8Status;

typedef struct UTF8Result {
    UTF8Status evt;
    uint8_t width;
    uint32_t pt;  // 0 if n/a
} UTF8Result;

UTF8Result utf8_decode(uint8_t *s, uint16_t inputlen);

#ifdef __cplusplus
}
#endif

#endif