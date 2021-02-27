#include <Arduino.h>
#include <avr/pgmspace.h>

#include "book.h"
#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pixelbuf.h"
#include "spi.h"
#include "utf8.h"
#include "util.h"

FATFS fs;
FRESULT res;
DIR dir;
FILINFO fno;

pixelbuf textrow = pixelbuf_new();
State state;
uint32_t offset = 0;

uint32_t buttonDownAt;  // millis of keydown start
uint8_t buttonState = HIGH;
uint8_t lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
// the debounce time; increase if the output flickers

void setup() {
    Serial.begin(9600);
    Serial.println(F("Starting..."));
    spi_begin();

    // SD card chip select
    pinMode(SD_CS_PIN, OUTPUT);

    // Set up button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    epd_init();
    epd_clear();

    disk_initialize();
    delay(100);

    res = pf_mount(&fs);
    if (res) {
        abort_with_ferror(res, &textrow);
    }

    res = pf_opendir(&dir, "/");
    if (res) {
        abort_with_ferror(res, &textrow);
    }

    // Read directory
    while (1) {
        res = pf_readdir(&dir, &fno);
        if (res) abort_with_ferror(res, &textrow);
        // end of listings
        if (!fno.fname[0]) abort_with_error(F("No ebooks found."), &textrow);

        Serial.print(fno.fname);
        Serial.print(F("\t"));
        Serial.print(fno.fext);
        Serial.print(F("\t"));
        Serial.println(fno.fsize);
        if (!strcmp(fno.fext, "TXT") && !(fno.fattrib & AM_DIR) &&
            !strcmp(fno.fname, FILENAME)) {
            break;
        }
    }

    // Open file
    res = pf_open(fno.fname);
    if (res) abort_with_ferror(res, &textrow);

    // For debugging end of book:
    // pf_lseek(428840);
    state = new_state(&fs, fno.fsize);
    // offset = show_offset(&state, 0, &textrow);
    // auto diff = show_offset(&state, 0, &textrow);
    // fs.fptr = diff;
    book_next_page(&state, &textrow);

    epd_slow_clockspeed();
    epd_set_lut(LUT_PARTIAL);
}

void loop() {
    // Serial.println("HUH");
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > 50) {
        if (reading != buttonState) {
            buttonState = reading;

            if (buttonState == LOW) {
                // Pressing
                buttonDownAt = millis();
            } else {
                // Releasing
                auto buttonDuration = millis() - buttonDownAt;
                if (buttonDuration <= 300) {
                    // Short press: Next page
                    book_next_page(&state, &textrow);
                }
            }
        } else if (reading == LOW) {
            if (millis() - buttonDownAt > 300) {
                book_prev_page(&state, &textrow);
            }
        }
    }

    // End of loop
    lastButtonState = reading;
}
