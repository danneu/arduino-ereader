#include <Arduino.h>

#include "config.h"
#include "epd.h"
#include "glyphs.h"
// #include "pff3a/source/diskio.h"
#include "book.h"
#include "pff3a/source/pff.h"
#include "pixelbuf.h"
#include "spi.h"
#include "utf8.h"
#include "util.h"

pixelbuf textrow = pixelbuf_new();
FATFS fs;
State state;
uint32_t offset = 0;

uint32_t buttonDownAt;  // millis of keydown start
uint8_t buttonState = HIGH;
uint8_t lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
// the debounce time; increase if the output flickers

void setup() {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    Serial.begin(9600);
    spi_begin();

    // SD card chip select
    pinMode(SD_CS_PIN, OUTPUT);

    // Set up button
    Serial.println("Starting...");
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    epd_init();
    epd_clear();

    disk_initialize();
    delay(100);

    res = pf_mount(&fs);
    if (res) {
        Serial.println(F("Failed to mount fs."));
        while (1)
            ;
    }

    res = pf_opendir(&dir, "/");
    if (res) {
        while (1)
            ;
    }

    // Read directory
    while (1) {
        res = pf_readdir(&dir, &fno);
        if (res != FR_OK) {
            serial1("Error reading directory.");
            while (1)
                ;
        }

        // end of listings
        if (!fno.fname[0]) {
            Serial.println(F("No ebook found."));
            while (true)
                ;
        }
        Serial.print(fno.fname);
        Serial.print(F("\t"));
        Serial.print(fno.fext);
        Serial.print(F("\t"));
        Serial.println(fno.fsize);
        if (!strcmp(fno.fext, "TXT") &&
            !strcmp(fno.fname, "ARRANC~2.TXT")) {  // && fno.fsize > 100) {
            // !strcmp(fno.fname, "LABIOS.TXT")) {  // && fno.fsize > 100) {
            // !strcmp(fno.fname, "TEST.TXT")) {
            break;
        }
    }

    // Open file
    res = pf_open(fno.fname);
    if (res) {
        while (1)
            ;
    }

    // For debugging end of book:
    // pf_lseek(428840);
    state = new_state(&fs, fno.fsize);
    // offset = show_offset(&state, 0, &textrow);
    // auto diff = show_offset(&state, 0, &textrow);
    // fs.fptr = diff;
    next_page(&state, &textrow);
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
                    // offset += show_offset(&state, offset, &textrow);
                    next_page(&state, &textrow);
                }
            }
        } else if (reading == LOW) {
            // Still holding down
            if (millis() - buttonDownAt > 300) {
                // offset -= 900;
                // show_offset(&state, offset, &textrow);
                prev_page(&state, &textrow);
            }
        }
    }

    // End of loop
    lastButtonState = reading;
}
