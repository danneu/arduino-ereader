#pragma once

#ifndef epd_h
#define epd_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EPD_LUT { LUT_FULL, LUT_PARTIAL } EPD_LUT;
typedef struct LUT {
    uint8_t vcom[44];
    uint8_t ww[42];
    uint8_t bw[42];
    uint8_t wb[42];
    uint8_t bb[42];
} LUT;

void epd_init();
void epd_clear();
void epd_refresh();
void epd_set_partial_window(const uint8_t* buf, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif

#endif  // guard