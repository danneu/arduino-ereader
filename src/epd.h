#pragma once

#include <stdint.h>

namespace epd {

// Some waveforms I've collected so far
enum CannedLUT { FullRefresh, PartialRefresh, QuickRefresh };

// Lookup table that tells the display how to handle refreshing
// Struct is 212 bytes total
struct WaveformLUT {
    uint8_t vcom[44];
    uint8_t ww[42];
    uint8_t bw[42];
    uint8_t wb[42];
    uint8_t bb[42];
};
enum Cmd {
    PANEL_SETTING = 0x00,
    POWER_SETTING = 0x01,
    POWER_OFF = 0x02,
    POWER_OFF_SEQUENCE_SETTING = 0x03,
    POWER_ON = 0x04,
    POWER_ON_MEASURE = 0x05,
    BOOSTER_SOFT_START = 0x06,
    DEEP_SLEEP = 0x07,
    DATA_START_TRANSMISSION_1 = 0x10,
    DATA_STOP = 0x11,
    DISPLAY_REFRESH = 0x12,
    DATA_START_TRANSMISSION_2 = 0x13,
    LUT_FOR_VCOM = 0x20,
    LUT_WHITE_TO_WHITE = 0x21,
    LUT_BLACK_TO_WHITE = 0x22,
    LUT_WHITE_TO_BLACK = 0x23,
    LUT_BLACK_TO_BLACK = 0x24,
    PLL_CONTROL = 0x30,
    TEMPERATURE_SENSOR_COMMAND = 0x40,
    TEMPERATURE_SENSOR_SELECTION = 0x41,
    TEMPERATURE_SENSOR_WRITE = 0x42,
    TEMPERATURE_SENSOR_READ = 0x43,
    VCOM_AND_DATA_INTERVAL_SETTING = 0x50,
    LOW_POWER_DETECTION = 0x51,
    TCON_SETTING = 0x60,
    RESOLUTION_SETTING = 0x61,
    GSST_SETTING = 0x65,
    GET_STATUS = 0x71,
    AUTO_MEASUREMENT_VCOM = 0x80,
    READ_VCOM_VALUE = 0x81,
    VCM_DC_SETTING = 0x82,
    PARTIAL_WINDOW = 0x90,
    PARTIAL_IN = 0x91,
    PARTIAL_OUT = 0x92,
    PROGRAM_MODE = 0xA0,
    ACTIVE_PROGRAMMING = 0xA1,
    READ_OTP = 0xA2,
    POWER_SAVING = 0xE3
};
void init();
void clear();
void reset();
void sendCommand(Cmd cmd);
void sendData(unsigned char byte);
void setLUT(CannedLUT lut);
void refreshDisplay();
void setPartialWindow(const unsigned char* buffer_black, int x, int y, int w,
                      int l);
}  // namespace epd