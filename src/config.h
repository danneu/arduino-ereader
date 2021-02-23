#pragma once

#ifndef config_h
#define config_h

#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8

#define SD_CS_PIN 5  // D5

#define EPD_RESET_PIN 8  // B0
#define EPD_DC_PIN 9     // B1
#define EPD_CS_PIN 10    // B2
#define EPD_BUSY_PIN 7   // D7

// #define PIN_LOW(port, pin) (port &= ~(1 << pin))
// #define PIN_HIGH(port, pin) (port |= (1 << pin))

// #define EPD_RESET_LOW() PIN_LOW(PORTB, DDB0)
// #define EPD_RESET_HIGH() PIN_HIGH(PORTB, DDB0)
// #define READ_BUSY()

#endif