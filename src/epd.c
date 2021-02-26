#include "epd.h"

#include <Arduino.h>
#include <avr/io.h>
#include <util/delay.h>

#include "config.h"
#include "spi.h"

#define CS_LOW() PORTB &= ~(1 << DDB2)
#define CS_HIGH() PORTB |= (1 << DDB2)

#define EPD_WIDTH 400
#define EPD_HEIGHT 300

// COMMANDS

#define PANEL_SETTING 0x00
#define POWER_SETTING 0x01
#define POWER_OFF 0x02
#define POWER_OFF_SEQUENCE_SETTING 0x03
#define POWER_ON 0x04
#define POWER_ON_MEASURE 0x05
#define BOOSTER_SOFT_START 0x06
#define DEEP_SLEEP 0x07
#define DATA_START_TRANSMISSION_1 0x10
#define DATA_STOP 0x11
#define DISPLAY_REFRESH 0x12
#define DATA_START_TRANSMISSION_2 0x13
#define LUT_FOR_VCOM 0x20
#define LUT_WHITE_TO_WHITE 0x21
#define LUT_BLACK_TO_WHITE 0x22
#define LUT_WHITE_TO_BLACK 0x23
#define LUT_BLACK_TO_BLACK 0x24
#define PLL_CONTROL 0x30
#define TEMPERATURE_SENSOR_COMMAND 0x40
#define TEMPERATURE_SENSOR_SELECTION 0x41
#define TEMPERATURE_SENSOR_WRITE 0x42
#define TEMPERATURE_SENSOR_READ 0x43
#define VCOM_AND_DATA_INTERVAL_SETTING 0x50
#define LOW_POWER_DETECTION 0x51
#define TCON_SETTING 0x60
#define RESOLUTION_SETTING 0x61
#define GSST_SETTING 0x65
#define GET_STATUS 0x71
#define AUTO_MEASUREMENT_VCOM 0x80
#define READ_VCOM_VALUE 0x81
#define VCM_DC_SETTING 0x82
#define PARTIAL_WINDOW 0x90
#define PARTIAL_IN 0x91
#define PARTIAL_OUT 0x92
#define PROGRAM_MODE 0xA0
#define ACTIVE_PROGRAMMING 0xA1
#define READ_OTP 0xA2
#define POWER_SAVING 0xE3

// E-ink display behavior can be changed with waveform lookup tables (LUTs)
// that tell the display how to handle transition between different pixel
// states.
//
// The Waveshare/Ingcool/GoodDisplay e-ink display (I think they are all
// identical) have two write-only pixel buffers in their own microcontroller
// that you can write to with the DATA_START_TRANSMISSION_{1,2} commands. Every
// time you send the the DISPLAY_REFRESH command to update the screen, the
// display updates new pixels with a function of fn(LUT, OLDBUFFER, NEWBUFFER).
// Once done, it copies the NEWBUFFER to be the new OLDBUFFER and you begin
// working on your next frame.
//
// I've copied some LUTs here that I've found in the wild or in various
// reference code that is good in some scenarios.

// LUT0 is for full refresh
static const uint8_t vcom0[] PROGMEM = {
    0x00, 0x17, 0x00, 0x00, 0x00, 0x02, 0x00, 0x17, 0x17, 0x00, 0x00,
    0x02, 0x00, 0x0A, 0x01, 0x00, 0x00, 0x01, 0x00, 0x0E, 0x0E, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t ww0[] PROGMEM = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00,
    0x02, 0x40, 0x0A, 0x01, 0x00, 0x00, 0x01, 0xA0, 0x0E, 0x0E, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t bw0[] PROGMEM = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00,
    0x02, 0x40, 0x0A, 0x01, 0x00, 0x00, 0x01, 0xA0, 0x0E, 0x0E, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t wb0[] PROGMEM = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00,
    0x02, 0x80, 0x0A, 0x01, 0x00, 0x00, 0x01, 0x50, 0x0E, 0x0E, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t bb0[] PROGMEM = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00,
    0x02, 0x80, 0x0A, 0x01, 0x00, 0x00, 0x01, 0x50, 0x0E, 0x0E, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// LUT1: partial/quick refresh

static const uint8_t vcom1[] PROGMEM = {
    0x00, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t ww1[] PROGMEM = {
    0x00, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t bw1[] PROGMEM = {
    0x80, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t wb1[] PROGMEM = {
    0x40, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t bb1[] PROGMEM = {
    0x00, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Resets the display hardware.
// This is the intended way to awake from deep sleep.
void epd_reset() {
    digitalWrite(EPD_RESET_PIN, LOW);
    _delay_ms(10);
    digitalWrite(EPD_RESET_PIN, HIGH);
    _delay_ms(10);
}

// Spin until busy pin is high again
void wait_until_idle() {
    while (!digitalRead(EPD_BUSY_PIN))
        ;
}

void epd_cmd(uint8_t cmd) {
    digitalWrite(EPD_CS_PIN, LOW);

    digitalWrite(EPD_DC_PIN, LOW);
    spi_xfer(cmd);
    digitalWrite(EPD_DC_PIN, HIGH);

    digitalWrite(EPD_CS_PIN, HIGH);
}

void epd_data(uint8_t data) {
    digitalWrite(EPD_CS_PIN, LOW);

    digitalWrite(EPD_DC_PIN, HIGH);
    spi_xfer(data);

    digitalWrite(EPD_CS_PIN, HIGH);
}

// Paints frame from SRAM to display
void epd_refresh() {
    epd_cmd(DISPLAY_REFRESH);
    _delay_ms(200);
    wait_until_idle();
}

// Whites out both of the display's internal pixel buffers
void epd_clear() {
    uint16_t i;
    epd_cmd(DATA_START_TRANSMISSION_1);
    for (i = 0; i < EPD_WIDTH / 8 * EPD_HEIGHT; i++) epd_data(0xff);
    epd_cmd(DATA_START_TRANSMISSION_2);
    for (i = 0; i < EPD_WIDTH / 8 * EPD_HEIGHT; i++) epd_data(0xff);

    epd_cmd(DISPLAY_REFRESH);
    _delay_ms(100);
    wait_until_idle();
}

void epd_set_lut(EPD_LUT lut) {
    uint8_t i;
    epd_cmd(LUT_FOR_VCOM);
    for (i = 0; i < 44; i++)
        epd_data(lut == LUT_PARTIAL ? pgm_read_byte(&vcom1[i])
                                    : pgm_read_byte(&vcom0[i]));
    epd_cmd(LUT_WHITE_TO_WHITE);
    for (i = 0; i < 42; i++)
        epd_data(lut == LUT_PARTIAL ? pgm_read_byte(&ww1[i])
                                    : pgm_read_byte(&ww0[i]));
    epd_cmd(LUT_BLACK_TO_WHITE);
    for (i = 0; i < 42; i++)
        epd_data(lut == LUT_PARTIAL ? pgm_read_byte(&bw1[i])
                                    : pgm_read_byte(&bw0[i]));
    epd_cmd(LUT_WHITE_TO_BLACK);
    for (i = 0; i < 42; i++)
        epd_data(lut == LUT_PARTIAL ? pgm_read_byte(&wb1[i])
                                    : pgm_read_byte(&wb0[i]));
    epd_cmd(LUT_BLACK_TO_BLACK);
    for (i = 0; i < 42; i++)
        epd_data(lut == LUT_PARTIAL ? pgm_read_byte(&bb1[i])
                                    : pgm_read_byte(&bb0[i]));
}

void epd_init() {
    pinMode(EPD_RESET_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_BUSY_PIN, INPUT);

    epd_reset();

    epd_cmd(POWER_SETTING);
    epd_data(0x03);
    epd_data(0x00);
    epd_data(0x2b);  // VDH
    epd_data(0x2b);  // VDL
    // epd_data(0xff);  // VDHR

    epd_cmd(BOOSTER_SOFT_START);
    epd_data(0x17);
    epd_data(0x17);
    epd_data(0x17);

    epd_cmd(POWER_ON);
    wait_until_idle();

    epd_cmd(PANEL_SETTING);
    epd_data(0xbf);
    epd_data(0x0d);
    // epd_data(0x3f);

    // Set display clock speed.
    // - 0x3c: 50MHz (default)
    // - 0x3a: 100MHz
    // - 0x29: 150MHz
    // - 0x31: 171MHz
    // - 0x39: 200MHz <-- Doesn't seem to work
    epd_cmd(PLL_CONTROL);
    epd_data(0x31);  // 171

    epd_cmd(RESOLUTION_SETTING);
    epd_data(EPD_WIDTH >> 8);
    epd_data(EPD_WIDTH & 0xff);
    epd_data(EPD_HEIGHT >> 8);
    epd_data(EPD_HEIGHT & 0xff);

    epd_cmd(VCM_DC_SETTING);
    epd_data(0x12);

    epd_cmd(VCOM_AND_DATA_INTERVAL_SETTING);
    epd_data(0x97);

    epd_set_lut(LUT_FULL);
}

// https://github.com/waveshare/e-Paper/blob/master/Arduino/epd4in2/epd4in2.cpp#L157
// In buffer, black pixels are the paiinnt
void epd_set_partial_window(const uint8_t* buf, int x, int y, int w, int h) {
    epd_cmd(PARTIAL_IN);
    epd_cmd(PARTIAL_WINDOW);
    epd_data(x >> 8);
    // x should be the multiple of 8, the last 3 bit will always be ignored
    epd_data(x & 0xf8);
    epd_data(((x & 0xf8) + w - 1) >> 8);
    epd_data(((x & 0xf8) + w - 1) | 0x07);
    epd_data(y >> 8);
    epd_data(y & 0xff);
    epd_data((y + h - 1) >> 8);
    epd_data((y + h - 1) & 0xff);
    // Gates scan both inside and outside of the partial window. (default)
    epd_data(0x01);
    _delay_ms(2);

    /* epd_send_command(DATA_START_TRANSMISSION_1); */
    /* for(int i=0; i<w/8*l; i++){ */
    /* epd_send_data(0xff); */
    /* } */

    epd_cmd(DATA_START_TRANSMISSION_2);
    for (uint16_t i = 0; i < w / 8 * h; i++) {
        epd_data(buf[i]);
    }
    _delay_ms(2);
    epd_cmd(PARTIAL_OUT);
}

// use epd_reset() to wake up.
void epd_deep_sleep() {
    epd_cmd(VCOM_AND_DATA_INTERVAL_SETTING);
    epd_cmd(VCM_DC_SETTING);  // VCOM to 0V
    epd_cmd(PANEL_SETTING);
    _delay_ms(100);

    // VG&VS to 0V faster
    epd_cmd(POWER_SETTING);
    for (uint8_t i = 0; i < 5; i++) {
        epd_data(0x00);
    }

    epd_cmd(POWER_OFF);
    wait_until_idle();
    epd_cmd(DEEP_SLEEP);
    epd_data(0xa5);
}

void epd_slow_clockspeed() {
    epd_cmd(PLL_CONTROL);
    epd_data(0x3c);  // 50
}

void epd_fast_clockspeed() {
    epd_cmd(PLL_CONTROL);
    epd_data(0x31);  // 171
}