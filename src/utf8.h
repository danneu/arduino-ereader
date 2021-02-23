#pragma once

#ifndef utf8_h
#define utf8_h

#include <stdint.h>

typedef enum {
    // 0: Successful decode
    UTF8_OK = 0,
    // 1: Invalid utf8 byte sequence
    UTF8_INVALID,
    // 2: Need more input, width will be how long the valid sequence
    // was until we hit EOF
    UTF8_EOF,
} UTF8_RESULT;

typedef struct DecodeResult {
    UTF8_RESULT evt;
    uint8_t width;
    long cp;  // -1 if n/a
} DecodeResult;

DecodeResult utf8_simple3(uint8_t *s, uint16_t inputlen) {
    DecodeResult d;
    if (inputlen < 1) {
        d = DecodeResult{.evt = UTF8_EOF, .width = 0, .cp = -1};
    } else if (s[0] < 0x80) {
        d = DecodeResult{.evt = UTF8_OK, .width = 1, .cp = s[0]};
    } else if ((s[0] & 0xe0) == 0xc0) {
        if (inputlen < 2) {
            d = DecodeResult{.evt = UTF8_EOF, .width = 1, .cp = -1};
        } else if ((s[1] & 0xc0) != 0x80) {
            d = DecodeResult{.evt = UTF8_INVALID, .width = 2, .cp = -1};
        } else {
            d = DecodeResult{.evt = UTF8_OK,
                             .width = 2,
                             .cp = ((uint32_t)(s[0] & 0x1f) << 6) |
                                   ((uint32_t)(s[1] & 0x3f) << 0)};
        }
    } else if ((s[0] & 0xf0) == 0xe0) {
        if (inputlen < 3) {
            d = DecodeResult{.evt = UTF8_EOF, .width = 2, .cp = -1};
        } else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) {
            d = DecodeResult{.evt = UTF8_INVALID, .width = 3, .cp = -1};
        } else {
            d = DecodeResult{.evt = UTF8_OK,
                             .width = 3,
                             .cp = ((uint32_t)(s[0] & 0x0f) << 12) |
                                   ((uint32_t)(s[1] & 0x3f) << 6) |
                                   ((uint32_t)(s[2] & 0x3f) << 0)};
        }
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        if (inputlen < 4) {
            d = DecodeResult{.evt = UTF8_EOF, .width = 3, .cp = -1};
        } else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                   (s[3] & 0xc0) != 0x80) {
            d = DecodeResult{.evt = UTF8_INVALID, .width = 4, .cp = -1};
        } else {
            d = DecodeResult{.evt = UTF8_OK,
                             .width = 4,
                             .cp = ((uint32_t)(s[0] & 0x07) << 18) |
                                   ((uint32_t)(s[1] & 0x3f) << 12) |
                                   ((uint32_t)(s[2] & 0x3f) << 6) |
                                   ((uint32_t)(s[3] & 0x3f) << 0)};
        }
    } else {
        d = DecodeResult{.evt = UTF8_INVALID, .width = 1, .cp = -1};
    }

    // invalid: surrogate half
    if (d.cp >= 0xd800 && d.cp <= 0xdfff) {
        d.evt = UTF8_INVALID;
    }

    return d;
}

// uint8_t utf8_simple2(uint8_t *s, uint32_t *c) {
//     unsigned char width;
//     if (s[0] < 0x80) {
//         *c = s[0];
//         width = 1;
//     } else if ((s[0] & 0xe0) == 0xc0) {
//         *c = ((uint32_t)(s[0] & 0x1f) << 6) | ((uint32_t)(s[1] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80) *c = -1;
//         width = 2;
//     } else if ((s[0] & 0xf0) == 0xe0) {
//         *c = ((uint32_t)(s[0] & 0x0f) << 12) | ((uint32_t)(s[1] & 0x3f) << 6)
//         |
//              ((uint32_t)(s[2] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) *c = -1;
//         width = 3;
//     } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
//         *c = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3f) <<
//         12) |
//              ((uint32_t)(s[2] & 0x3f) << 6) | ((uint32_t)(s[3] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
//             (s[3] & 0xc0) != 0x80)
//             *c = -1;
//         width = 4;
//     } else {
//         *c = -1;    // invalid
//         width = 1;  // skip this byte
//     }
//     if (*c >= 0xd800 && *c <= 0xdfff) *c = -1;  // surrogate half
//     return width;
// }

// uint8_t *utf8_simple(uint8_t *s, uint32_t *c) {
//     unsigned char *next;
//     if (s[0] < 0x80) {
//         *c = s[0];
//         next = s + 1;
//     } else if ((s[0] & 0xe0) == 0xc0) {
//         *c = ((uint32_t)(s[0] & 0x1f) << 6) | ((uint32_t)(s[1] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80) *c = -1;
//         next = s + 2;
//     } else if ((s[0] & 0xf0) == 0xe0) {
//         *c = ((uint32_t)(s[0] & 0x0f) << 12) | ((uint32_t)(s[1] & 0x3f) << 6)
//         |
//              ((uint32_t)(s[2] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) *c = -1;
//         next = s + 3;
//     } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
//         *c = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3f) <<
//         12) |
//              ((uint32_t)(s[2] & 0x3f) << 6) | ((uint32_t)(s[3] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
//             (s[3] & 0xc0) != 0x80)
//             *c = -1;
//         next = s + 4;
//     } else {
//         *c = -1;       // invalid
//         next = s + 1;  // skip this byte
//     }
//     if (*c >= 0xd800 && *c <= 0xdfff) *c = -1;  // surrogate half
//     return next;
// }

#endif