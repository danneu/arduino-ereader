#ifndef _glyphs_h
#define _glyphs_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int get_glyph(uint32_t codepoint, uint8_t idx, uint8_t *buf);
int get_glyph2(uint32_t codepoint, uint8_t idx, uint8_t *buf);
int get_full_glyph(uint32_t codepoint, uint8_t buf[16]);

#ifdef __cplusplus
}
#endif

#endif  // dbl guard