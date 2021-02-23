#ifndef glyphs_h
#define glyphs_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t get_glyph(uint32_t codepoint, uint8_t buf[16]);

#ifdef __cplusplus
}
#endif

#endif  // guard