#include "util.h"

static unsigned char lookup[16] = {
    0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
    0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

uint8_t reversebits(uint8_t n) {
    // Reverse the top and bottom nibble then swap them.
    return (lookup[n & 0b1111] << 4) | lookup[n >> 4];
}

void transpose8(unsigned char A[8], int32_t m, int32_t n, unsigned char B[8]) {
    uint32_t x, y, t;

    // Load the array and pack it into x and y.

    x = ((long)A[0] << 24) | ((long)A[m] << 16) | (A[2 * m] << 8) | A[3 * m];
    y = ((long)A[4 * m] << 24) | ((long)A[5 * m] << 16) | (A[6 * m] << 8) |
        A[7 * m];

    t = (x ^ (x >> 7)) & 0x00AA00AA;
    x = x ^ t ^ (t << 7);
    t = (y ^ (y >> 7)) & 0x00AA00AA;
    y = y ^ t ^ (t << 7);

    t = (x ^ (x >> 14)) & 0x0000CCCC;
    x = x ^ t ^ (t << 14);
    t = (y ^ (y >> 14)) & 0x0000CCCC;
    y = y ^ t ^ (t << 14);

    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    B[0] = x >> 24;
    B[n] = x >> 16;
    B[2 * n] = x >> 8;
    B[3 * n] = x;
    B[4 * n] = y >> 24;
    B[5 * n] = y >> 16;
    B[6 * n] = y >> 8;
    B[7 * n] = y;
}