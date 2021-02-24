#ifndef config_h
#define config_h

#define GLYPHS_ON 1

#define LEFT_MARGIN (1 * CHAR_WIDTH)
#define WIDTH (400 - LEFT_MARGIN)
#define HEIGHT 300
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16
#define TEXTROW_BUFSIZE (CHAR_HEIGHT * WIDTH / CHAR_WIDTH)

#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8
#define ROWS_PER_PAGE (uint8_t)(HEIGHT / CHAR_HEIGHT)
#define CHARS_PER_ROW (uint8_t)(WIDTH / CHAR_WIDTH)

#define SD_CS_PIN 5  // D5

#define EPD_RESET_PIN 8  // B0
#define EPD_DC_PIN 9     // B1
#define EPD_CS_PIN 10    // B2
#define EPD_BUSY_PIN 7   // D7

#endif