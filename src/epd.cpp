#include "epd.h"

#include <SPI.h>

#define EPD_RESET_PIN 8  // B0
#define EPD_DC_PIN 9     // B1
#define EPD_CS_PIN 10    // B2
#define EPD_BUSY_PIN 7   // D7

// Drive chipSelect (SPI slave toggle)
// TODO: Come up with better solution
#define EPD_ENABLE() digitalWrite(EPD_CS_PIN, LOW)
#define EPD_DISABLE() digitalWrite(EPD_CS_PIN, HIGH)
// #define SPI_SETTINGS SPISettings(2000000, MSBFIRST, SPI_MODE0)
#define SPI_SETTINGS SPISettings()

#define EPD_WIDTH 400
#define EPD_HEIGHT 300

using namespace epd;

namespace epd {

// Resets the display hardware.
// This is the intended way to awake from deep sleep.
void reset() {
    digitalWrite(EPD_RESET_PIN, LOW);
    delay(200);
    digitalWrite(EPD_RESET_PIN, HIGH);
    delay(200);
}

// Wait util busy pin is high again
void waitUntilIdle() {
    while (!digitalRead(EPD_BUSY_PIN))
        ;
    // while (digitalRead(EPD_BUSY_PIN) == HIGH) ;
}

uint8_t spiTransmit(uint8_t data) {
    // load data into register
    SPDR = data;
    // wait until trasferred
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

// TODO chipselect on send_{cmd,dta}
void sendCommand(Cmd cmd) {
    // SPI.beginTransaction(SPISettings());
    // SPI.beginTransaction(SPI_SETTINGS);
    EPD_ENABLE();

    digitalWrite(EPD_DC_PIN, LOW);
    SPI.transfer(cmd);
    digitalWrite(EPD_DC_PIN, HIGH);

    EPD_DISABLE();
    // SPI.endTransaction();
}

void sendData(uint8_t data) {
    // SPI.beginTransaction(SPISettings());
    // SPI.beginTransaction(SPI_SETTINGS);
    EPD_ENABLE();

    digitalWrite(EPD_DC_PIN, HIGH);
    SPI.transfer(data);

    EPD_DISABLE();
    // SPI.endTransaction();
}

// Paints frame from SRAM to display
void refreshDisplay() {
    sendCommand(Cmd::DISPLAY_REFRESH);
    delay(200);
    waitUntilIdle();
}

// Whites out both of the display's internal pixel buffers
void clear() {
    sendCommand(Cmd::DATA_START_TRANSMISSION_1);
    for (int y = 0; y < EPD_HEIGHT; y++) {
        for (int x = 0; x < EPD_WIDTH / 8; x++) {
            sendData(0xff);
        }
    }

    sendCommand(Cmd::DATA_START_TRANSMISSION_2);
    for (int y = 0; y < EPD_HEIGHT; y++) {
        for (int x = 0; x < EPD_WIDTH / 8; x++) {
            sendData(0xff);
        }
    }

    sendCommand(Cmd::DISPLAY_REFRESH);
    delay(100);
    waitUntilIdle();
}

// https://github.com/waveshare/e-Paper/blob/master/Arduino/epd4in2/epd4in2.cpp#L157
void setPartialWindow(const unsigned char* buffer_black, int x, int y, int w,
                      int l) {
    sendCommand(Cmd::PARTIAL_IN);
    sendCommand(Cmd::PARTIAL_WINDOW);
    sendData(x >> 8);
    // x should be the multiple of 8, the last 3 bit will always be ignored
    sendData(x & 0xf8);
    sendData(((x & 0xf8) + w - 1) >> 8);
    sendData(((x & 0xf8) + w - 1) | 0x07);
    sendData(y >> 8);
    sendData(y & 0xff);
    sendData((y + l - 1) >> 8);
    sendData((y + l - 1) & 0xff);
    // Gates scan both inside and outside of the partial window. (default)
    sendData(0x01);
    delay(2);

    /* epd_send_command(DATA_START_TRANSMISSION_1); */
    /* for(int i=0; i<w/8*l; i++){ */
    /* epd_send_data(0xff); */
    /* } */

    sendCommand(Cmd::DATA_START_TRANSMISSION_2);
    for (int i = 0; i < w / 8 * l; i++) {
        sendData(buffer_black[i]);
    }
    delay(2);
    sendCommand(Cmd::PARTIAL_OUT);
}

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

void setLUTWaveform(WaveformLUT lut) {
    sendCommand(Cmd::LUT_FOR_VCOM);
    for (int i = 0; i < 44; i++) sendData(lut.vcom[i]);
    sendCommand(Cmd::LUT_WHITE_TO_WHITE);
    for (int i = 0; i < 42; i++) sendData(lut.ww[i]);
    sendCommand(Cmd::LUT_BLACK_TO_WHITE);
    for (int i = 0; i < 42; i++) sendData(lut.bw[i]);
    sendCommand(Cmd::LUT_WHITE_TO_BLACK);
    for (int i = 0; i < 42; i++) sendData(lut.wb[i]);
    sendCommand(Cmd::LUT_BLACK_TO_BLACK);
    for (int i = 0; i < 42; i++) sendData(lut.bb[i]);
}

// Commenting this function body out saves me 212 bytes of global variable
// allocation (the size of the WaveformLUT struct)
void setLUT(CannedLUT lut) {
    switch (lut) {
        case FullRefresh:
            return setLUTWaveform(WaveformLUT{
                .vcom =
                    {
                        0x00, 0x17, 0x00, 0x00, 0x00, 0x02, 0x00, 0x17, 0x17,
                        0x00, 0x00, 0x02, 0x00, 0x0A, 0x01, 0x00, 0x00, 0x01,
                        0x00, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .ww =
                    {
                        0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17,
                        0x00, 0x00, 0x02, 0x40, 0x0A, 0x01, 0x00, 0x00, 0x01,
                        0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .bw =
                    {
                        0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17,
                        0x00, 0x00, 0x02, 0x40, 0x0A, 0x01, 0x00, 0x00, 0x01,
                        0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .wb =
                    {
                        0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17,
                        0x00, 0x00, 0x02, 0x80, 0x0A, 0x01, 0x00, 0x00, 0x01,
                        0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .bb =
                    {
                        0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17,
                        0x00, 0x00, 0x02, 0x80, 0x0A, 0x01, 0x00, 0x00, 0x01,
                        0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    }

            });
        case PartialRefresh:
            return setLUTWaveform(WaveformLUT{
                .vcom = {0x00, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

                .ww = {0x00, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

                .bw = {0x80, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

                .wb =
                    {
                        0x40, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },

                .bb =
                    {
                        0x00, 0x19, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    }

            });
        case QuickRefresh:
            return setLUTWaveform(WaveformLUT{
                .vcom =
                    {
                        0x00, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .ww =
                    {
                        0xA0, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .bw =
                    {
                        0xA0, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .wb =
                    {
                        0x50, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },
                .bb =
                    {
                        0x50, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    },

            });
    }
}

void init() {
    // Config pin I/O
    pinMode(EPD_RESET_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_CS_PIN, OUTPUT);  // slave select
    pinMode(EPD_BUSY_PIN, INPUT);

    reset();

    // Init handshake
    sendCommand(Cmd::POWER_SETTING);
    sendData(0x03);
    sendData(0x00);
    sendData(0x2b);
    sendData(0x2b);
    // sendData(0xff);

    sendCommand(Cmd::BOOSTER_SOFT_START);
    sendData(0x17);
    sendData(0x17);
    sendData(0x17);

    sendCommand(Cmd::POWER_ON);
    waitUntilIdle();

    sendCommand(Cmd::PANEL_SETTING);
    sendData(0xbf);
    sendData(0x0d);

    // Set display clockspeed (default 50Mhz)
    // I couldn't get 200Mhz (max) to refresh the screen,
    // but 150Mhz and 100Mhz work well.
    sendCommand(Cmd::PLL_CONTROL);
    sendData(0x3a);  // 100Mhz TODO: Change to 150Mhz, nice for dev.

    sendCommand(Cmd::RESOLUTION_SETTING);
    sendData(EPD_WIDTH >> 8);
    sendData(EPD_WIDTH & 0xff);
    sendData(EPD_HEIGHT >> 8);
    sendData(EPD_HEIGHT & 0xff);

    sendCommand(Cmd::VCM_DC_SETTING);
    sendData(0x12);
    sendCommand(Cmd::VCOM_AND_DATA_INTERVAL_SETTING);
    sendData(0x97);

    setLUT(CannedLUT::FullRefresh);
}

}  // namespace epd