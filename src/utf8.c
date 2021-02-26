#include "utf8.h"

UTF8Result utf8_decode(uint8_t *s, uint16_t inputlen) {
    UTF8Result d;
    if (inputlen < 1) {
        d = (UTF8Result){.evt = UTF8_EOI, .width = 0, .pt = 0};
    } else if (s[0] < 0x80) {
        d = (UTF8Result){.evt = UTF8_OK, .width = 1, .pt = s[0]};
    } else if ((s[0] & 0xe0) == 0xc0) {
        if (inputlen < 2) {
            d = (UTF8Result){.evt = UTF8_EOI, .width = 1, .pt = 0};
        } else if ((s[1] & 0xc0) != 0x80) {
            d = (UTF8Result){.evt = UTF8_INVALID, .width = 2, .pt = 0};
        } else {
            d = (UTF8Result){.evt = UTF8_OK,
                             .width = 2,
                             .pt = ((uint32_t)(s[0] & 0x1f) << 6) |
                                   ((uint32_t)(s[1] & 0x3f) << 0)};
        }
    } else if ((s[0] & 0xf0) == 0xe0) {
        if (inputlen < 3) {
            d = (UTF8Result){.evt = UTF8_EOI, .width = 2, .pt = 0};
        } else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80) {
            d = (UTF8Result){.evt = UTF8_INVALID, .width = 3, .pt = 0};
        } else {
            d = (UTF8Result){.evt = UTF8_OK,
                             .width = 3,
                             .pt = ((uint32_t)(s[0] & 0x0f) << 12) |
                                   ((uint32_t)(s[1] & 0x3f) << 6) |
                                   ((uint32_t)(s[2] & 0x3f) << 0)};
        }
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        if (inputlen < 4) {
            d = (UTF8Result){.evt = UTF8_EOI, .width = 3, .pt = 0};
        } else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                   (s[3] & 0xc0) != 0x80) {
            d = (UTF8Result){.evt = UTF8_INVALID, .width = 4, .pt = 0};
        } else {
            d = (UTF8Result){.evt = UTF8_OK,
                             .width = 4,
                             .pt = ((uint32_t)(s[0] & 0x07) << 18) |
                                   ((uint32_t)(s[1] & 0x3f) << 12) |
                                   ((uint32_t)(s[2] & 0x3f) << 6) |
                                   ((uint32_t)(s[3] & 0x3f) << 0)};
        }
    } else {
        d = (UTF8Result){.evt = UTF8_INVALID, .width = 1, .pt = 0};
    }

    // invalid: surrogate half
    if (d.pt >= 0xd800 && d.pt <= 0xdfff) {
        d.evt = UTF8_INVALID;
    }

    return d;
}
