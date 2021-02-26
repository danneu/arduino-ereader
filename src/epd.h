#ifndef epd_h
#define epd_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EPD_LUT { LUT_FULL, LUT_PARTIAL } EPD_LUT;

void epd_init();
void epd_reset();
void epd_clear();
void epd_refresh();
void epd_set_lut(EPD_LUT lut);
void epd_set_partial_window(const uint8_t* buf, int x, int y, int w, int h);
void epd_deep_sleep();
void epd_fast_clockspeed();
void epd_slow_clockspeed();

#ifdef __cplusplus
}
#endif

#endif  // guard