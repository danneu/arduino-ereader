#ifndef _glyphs_h
#define _glyphs_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t get_glyph(uint32_t codepoint, uint8_t buf[16]);

#ifdef __cplusplus
}
#endif

#endif  // dbl guard