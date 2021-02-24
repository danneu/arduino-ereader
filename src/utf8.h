#pragma once

#ifndef utf8_h
#define utf8_h

#include <stdint.h>

typedef enum {
    // 0: Successful decode. `width` tells how many bytes it took
    // to decode the codepoint.
    UTF8_OK = 0,
    // 1: Invalid utf8 byte sequence. `width` will be the length
    // of it, just skip it.
    UTF8_INVALID,
    // 2: Ran out of input. `width` will be
    // the length of valid sequence we consumed when we hit EOI.
    UTF8_EOI,
} UTF8_STATUS;

typedef struct UTF8_RESULT {
    UTF8_STATUS evt;
    uint8_t width;
    uint32_t pt;  // 0 if n/a
} UTF8_RESULT;

UTF8_RESULT utf8_decode(uint8_t *s, uint16_t inputlen) {
    // Serial.print("inputlen: ");
    // Serial.println(inputlen);
    UTF8_RESULT d;
    if (inputlen < 1) {
        Serial.println("in <0 prt of utf8 dec");
        d = UTF8_RESULT{.evt = UTF8_EOI, .width = 0, .pt = 0};
    } else if (s[0] < 0x80) {
        d = UTF8_RESULT{.evt = UTF8_OK, .width = 1, .pt = s[0]};
    } else if ((s[0] & 0xe0) == 0xc0) {
        if (inputlen < 2) {
            d = UTF8_RESULT{.evt = UTF8_EOI, .width = 1, .pt = 0};
        } else if ((s[1] & 0xc0) != 0x80) {
            d = UTF8_RESULT{.evt = UTF8_INVALID, .width = 2, .pt = 0};
        } else {
            d = UTF8_RESULT{.evt = UTF8_OK,
                            .width = 2,
                            .pt = ((uint32_t)(s[0] & 0x1f) << 6) |
                                  ((uint32_t)(s[1] & 0x3f) << 0)};
        }
    } else if ((s[0] & 0xf0) == 0xe0) {
        if (inputlen < 3) {
            d = UTF8_RESULT{.evt = UTF8_EOI, .width = 2, .pt = 0};
        } else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) {
            d = UTF8_RESULT{.evt = UTF8_INVALID, .width = 3, .pt = 0};
        } else {
            d = UTF8_RESULT{.evt = UTF8_OK,
                            .width = 3,
                            .pt = ((uint32_t)(s[0] & 0x0f) << 12) |
                                  ((uint32_t)(s[1] & 0x3f) << 6) |
                                  ((uint32_t)(s[2] & 0x3f) << 0)};
        }
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        if (inputlen < 4) {
            d = UTF8_RESULT{.evt = UTF8_EOI, .width = 3, .pt = 0};
        } else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                   (s[3] & 0xc0) != 0x80) {
            d = UTF8_RESULT{.evt = UTF8_INVALID, .width = 4, .pt = 0};
        } else {
            d = UTF8_RESULT{.evt = UTF8_OK,
                            .width = 4,
                            .pt = ((uint32_t)(s[0] & 0x07) << 18) |
                                  ((uint32_t)(s[1] & 0x3f) << 12) |
                                  ((uint32_t)(s[2] & 0x3f) << 6) |
                                  ((uint32_t)(s[3] & 0x3f) << 0)};
        }
    } else {
        d = UTF8_RESULT{.evt = UTF8_INVALID, .width = 1, .pt = 0};
    }

    // invalid: surrogate half
    if (d.pt >= 0xd800 && d.pt <= 0xdfff) {
        d.evt = UTF8_INVALID;
    }

    return d;
}

#endif