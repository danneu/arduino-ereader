#pragma once
/* Branchless UTF-8 decoder
 *
 * This is free and unencumbered software released into the public domain.
 */
#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

/* Results of Disk Functions */

// inputlen tells us how many bytes we can read from input `s` so that we can
// communicate that we need more bytes.
// uint8_t *utf8_simple(uint8_t *s, uint16_t inputlen, long *cp,
//                      UTF8_RESULT *res) {
//     unsigned char *next;
//     if (inputlen < 1) {
//         *res = UTF8_EOF;
//         next = s;
//     } else if (s[0] < 0x80) {  // inputlen is at least 1
//         *cp = s[0];
//         *res = UTF8_OK;
//         next = s + 1;
//     } else if ((s[0] & 0xe0) == 0xc0) {
//         *cp = ((long)(s[0] & 0x1f) << 6) | ((long)(s[1] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80) {
//             *cp = -1;
//         }
//         next = s + 2;
//     } else if ((s[0] & 0xf0) == 0xe0) {
//         *cp = ((long)(s[0] & 0x0f) << 12) | ((long)(s[1] & 0x3f) << 6) |
//               ((long)(s[2] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) *cp = -1;
//         next = s + 3;
//     } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
//         *cp = ((long)(s[0] & 0x07) << 18) | ((long)(s[1] & 0x3f) << 12) |
//               ((long)(s[2] & 0x3f) << 6) | ((long)(s[3] & 0x3f) << 0);
//         if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
//             (s[3] & 0xc0) != 0x80)
//             *cp = -1;
//         next = s + 4;
//     } else {
//         *cp = -1;      // invalid
//         next = s + 1;  // skip this byte
//     }
//     if (*cp >= 0xd800 && *cp <= 0xdfff) *cp = -1;  // surrogate half
//     return next;
// }

typedef enum {
    UTF8_OK = 0,   // 0: Function succeeded
    UTF8_INVALID,  // 1: Invalid byte sequence to decode a codepoint
                   // next pointer is advanced
    UTF8_EOF,  // 2: Need more input, width will be how long the valid sequence
               // was until we hit EOF
} UTF8_RESULT;

typedef struct DecodeResult {
    UTF8_RESULT evt;
    uint8_t width;
    long cp;  // -1 if n/a
} DecodeResult;

DecodeResult utf8_simple3(uint8_t *s, uint16_t inputlen) {
    if (inputlen < 1) {
        return DecodeResult{.evt = UTF8_EOF, .width = 0, .cp = -1};
    } else if (s[0] < 0x80) {  // inputlen is >= 1
        return DecodeResult{.evt = UTF8_OK, .width = 1, .cp = s[0]};
    } else if ((s[0] & 0xe0) == 0xc0) {
        if (inputlen < 2) {
            return DecodeResult{.evt = UTF8_EOF, .width = 1, .cp = -1};
        }
        if ((s[1] & 0xc0) != 0x80) {
            return DecodeResult{.evt = UTF8_INVALID, .width = 2, .cp = -1};
        }
        return DecodeResult{.evt = UTF8_OK,
                            .width = 2,
                            .cp = ((uint32_t)(s[0] & 0x1f) << 6) |
                                  ((uint32_t)(s[1] & 0x3f) << 0)};
    } else if ((s[0] & 0xf0) == 0xe0) {
        if (inputlen < 3) {
            return DecodeResult{.evt = UTF8_EOF, .width = 2, .cp = -1};
        }
        if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) {
            return DecodeResult{.evt = UTF8_INVALID, .width = 3, .cp = -1};
        }
        return DecodeResult{.evt = UTF8_OK,
                            .width = 3,
                            .cp = ((uint32_t)(s[0] & 0x0f) << 12) |
                                  ((uint32_t)(s[1] & 0x3f) << 6) |
                                  ((uint32_t)(s[2] & 0x3f) << 0)};

    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        if (inputlen < 4) {
            return DecodeResult{.evt = UTF8_EOF, .width = 3, .cp = -1};
        }
        if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
            (s[3] & 0xc0) != 0x80) {
            return DecodeResult{.evt = UTF8_INVALID, .width = 4, .cp = -1};
        }
        return DecodeResult{.evt = UTF8_OK,
                            .width = 4,
                            .cp = ((uint32_t)(s[0] & 0x07) << 18) |
                                  ((uint32_t)(s[1] & 0x3f) << 12) |
                                  ((uint32_t)(s[2] & 0x3f) << 6) |
                                  ((uint32_t)(s[3] & 0x3f) << 0)};

    } else {
        return DecodeResult{.evt = UTF8_INVALID, .width = 1, .cp = -1};
    }
    // invalid surrogate half
    if (*c >= 0xd800 && *c <= 0xdfff) *c = -1;  // surrogate half
    return width;
}

uint8_t utf8_simple2(uint8_t *s, uint32_t *c) {
    unsigned char width;
    if (s[0] < 0x80) {
        *c = s[0];
        width = 1;
    } else if ((s[0] & 0xe0) == 0xc0) {
        *c = ((uint32_t)(s[0] & 0x1f) << 6) | ((uint32_t)(s[1] & 0x3f) << 0);
        if ((s[1] & 0xc0) != 0x80) *c = -1;
        width = 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
        *c = ((uint32_t)(s[0] & 0x0f) << 12) | ((uint32_t)(s[1] & 0x3f) << 6) |
             ((uint32_t)(s[2] & 0x3f) << 0);
        if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) *c = -1;
        width = 3;
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        *c = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3f) << 12) |
             ((uint32_t)(s[2] & 0x3f) << 6) | ((uint32_t)(s[3] & 0x3f) << 0);
        if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
            (s[3] & 0xc0) != 0x80)
            *c = -1;
        width = 4;
    } else {
        *c = -1;    // invalid
        width = 1;  // skip this byte
    }
    if (*c >= 0xd800 && *c <= 0xdfff) *c = -1;  // surrogate half
    return width;
}

uint8_t *utf8_simple(uint8_t *s, uint32_t *c) {
    unsigned char *next;
    if (s[0] < 0x80) {
        *c = s[0];
        next = s + 1;
    } else if ((s[0] & 0xe0) == 0xc0) {
        *c = ((uint32_t)(s[0] & 0x1f) << 6) | ((uint32_t)(s[1] & 0x3f) << 0);
        if ((s[1] & 0xc0) != 0x80) *c = -1;
        next = s + 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
        *c = ((uint32_t)(s[0] & 0x0f) << 12) | ((uint32_t)(s[1] & 0x3f) << 6) |
             ((uint32_t)(s[2] & 0x3f) << 0);
        if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) *c = -1;
        next = s + 3;
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        *c = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3f) << 12) |
             ((uint32_t)(s[2] & 0x3f) << 6) | ((uint32_t)(s[3] & 0x3f) << 0);
        if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
            (s[3] & 0xc0) != 0x80)
            *c = -1;
        next = s + 4;
    } else {
        *c = -1;       // invalid
        next = s + 1;  // skip this byte
    }
    if (*c >= 0xd800 && *c <= 0xdfff) *c = -1;  // surrogate half
    return next;
}

// uint8_t *utf8_simpleOLD(
//     uint8_t *s,   // buffer of bytes to decode
//     uint32_t *cp  // length of decoded glyph (will be -1 if bad)
// ) {
//     unsigned char *next;
//     if (s[0] < 0x80) {
//         *cp = s[0];
//         next = s + 1;
//     } else if ((s[0] & 0xe0) == 0xc0) {
//         *cp = ((long)(s[0] & 0x1f) << 6) | ((long)(s[1] & 0x3f) << 0);
//         next = s + 2;
//     } else if ((s[0] & 0xf0) == 0xe0) {
//         *cp = ((long)(s[0] & 0x0f) << 12) | ((long)(s[1] & 0x3f) << 6) |
//               ((long)(s[2] & 0x3f) << 0);
//         next = s + 3;
//     } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
//         *cp = ((long)(s[0] & 0x07) << 18) | ((long)(s[1] & 0x3f) << 12) |
//               ((long)(s[2] & 0x3f) << 6) | ((long)(s[3] & 0x3f) << 0);
//         next = s + 4;
//     } else {
//         *cp = -1;      // invalid
//         next = s + 1;  // skip this byte
//     }
//     if (*cp >= 0xd800 && *cp <= 0xdfff) *cp = -1;  // surrogate half
//     return next;
// }

/* Decode the next character, C, from BUF, reporting errors in E.
 *
 * Since this is a branchless decoder, four bytes will be read from the
 * buffer regardless of the actual length of the next character. This
 * means the buffer _must_ have at least three bytes of zero padding
 * following the end of the data stream.
 *
 * Errors are reported in E, which will be non-zero if the parsed
 * character was somehow invalid: invalid byte sequence, non-canonical
 * encoding, or a surrogate half.
 *
 * The function returns a pointer to the next character. When an error
 * occurs, this pointer will be a guess that depends on the particular
 * error, but it will always advance at least one byte.
 */
// static void *utf8_decode(void *buf, uint32_t *c, int *e) {
//     static const char lengths[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//                                    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
//                                    0, 0, 2, 2, 2, 2, 3, 3, 4, 0};
//     static const int masks[] = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
//     static const uint32_t mins[] = {4194304, 0, 128, 2048, 65536};
//     static const int shiftc[] = {0, 18, 12, 6, 0};
//     static const int shifte[] = {0, 6, 4, 2, 0};

//     unsigned char *s = buf;
//     int len = lengths[s[0] >> 3];

//     /* Compute the pointer to the next character early so that the next
//      * iteration can start working on the next character. Neither Clang
//      * nor GCC figure out this reordering on their own.
//      */
//     unsigned char *next = s + len + !len;

//     /* Assume a four-byte character and load four bytes. Unused bits are
//      * shifted out.
//      */
//     *c = (uint32_t)(s[0] & masks[len]) << 18;
//     *c |= (uint32_t)(s[1] & 0x3f) << 12;
//     *c |= (uint32_t)(s[2] & 0x3f) << 6;
//     *c |= (uint32_t)(s[3] & 0x3f) << 0;
//     *c >>= shiftc[len];

//     /* Accumulate the various error conditions. */
//     *e = (*c < mins[len]) << 6;       // non-canonical encoding
//     *e |= ((*c >> 11) == 0x1b) << 7;  // surrogate half?
//     *e |= (*c > 0x10FFFF) << 8;       // out of range?
//     *e |= (s[1] & 0xc0) >> 2;
//     *e |= (s[2] & 0xc0) >> 4;
//     *e |= (s[3]) >> 6;
//     *e ^= 0x2a;  // top two bits of each tail byte correct?
//     *e >>= shifte[len];

//     return next;
// }

// #define UTF8_OK 0
// #define UTF8_INVALID 1

// int utf8_decode(uint8_t *input, uint32_t *cp, uint8_t *width) {
//     *width = 0;  // reset
//     int result = 0;
//     // int input[6] = {};

//     if (input[0] < 0x80) {
//         // the first character is the only 7 bit sequence...
//         *width = 1;
//         *cp = input[0];
//         return UTF8_OK;
//     } else if ((input[0] & 0xC0) == 0x80) {
//         // This is not the beginning of the multibyte sequence.
//         return UTF8_INVALID;
//     } else if ((input[0] & 0xfe) == 0xfe) {
//         // This is not a valid UTF-8 stream.
//         return UTF8_INVALID;
//     } else {
//         for (int sequence_length = 1; input[0] & (0x80 >> sequence_length);
//              ++sequence_length)
//             ;
//         result = input[0] & ((1 << sequence_length) - 1);
//         printf("squence length = %d ", sequence_length);
//         int index;
//         for (index = 1; index < sequence_length; ++index) {
//             input[index] = fgetc(f);
//             printf("(i[%d] = %d) ", index, input[index]);
//             if (input[index] == EOF) {
//                 return EOF;
//             }
//             result = (result << 6) | (input[index] & 0x30);
//         }
//     }
//     return result;
// }

// main(int argc, char **argv) {
//     printf("open(%s) ", argv[1]);
//     FILE *f = fopen(argv[1], "r");
//     int c = 0;
//     while (c != EOF) {
//         c = fgetutf8c(f);
//         printf("* %d\n", c);
//     }
//     fclose(f);
// }

#endif