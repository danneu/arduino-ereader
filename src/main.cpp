#include <Arduino.h>
#include <SPI.h>

#include "epd.h"
#include "glyphs.h"
#include "pff3a/source/diskio.h"
#include "pff3a/source/pff.h"
#include "utf8.h"

// TODO: http://elm-chan.org/docs/mmc/mmc_e.html
// Impl examples: https://gist.github.com/RickKimball/2325039

static void print_fresult(FRESULT rc) {
    static const char str[8][15] = {
        "OK            ", "DISK_ERR      ", "NOT_READY     ", "NO_FILE       ",
        "NO_PATH       ", "NOT_OPENED    ", "NOT_ENABLED   ", "NO_FILE_SYSTEM"};
    char buf[9 + 14];
    sprintf(buf, "FRESULT: %s", str[rc]);
    Serial.println(buf);
}

bool isTxtFile(FILINFO *f) {
    return f->fname[11] == 'T' && f->fname[10] == 'X' && f->fname[9] == 'T' &&
           f->fname[8] == '.';  // && f->fattrib & AM_DIR == 0;
}

#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8
#define WIDTH 400
#define HEIGHT 300
#define CHARS_PER_ROW WIDTH / CHAR_WIDTH
#define ROWS_PER_PAGE HEIGHT / (CHAR_HEIGHT + LINE_SPACING)
#define CHARS_PER_PAGE CHARS_PER_ROW *ROWS_PER_PAGE

#define TEXTROW_BUFSIZE CHAR_HEIGHT *WIDTH / CHAR_WIDTH
uint8_t textrow[TEXTROW_BUFSIZE] = {0xff};

void textrow_clear(uint8_t *frame) {
    for (uint16_t i = 0; i < TEXTROW_BUFSIZE; i++) {
        frame[i] = 0xff;
    }
    // uint16_t y, x;
    // for (y = 0; y < CHAR_HEIGHT; y++) {
    //     for (x = 0; x < WIDTH / CHAR_WIDTH; x++) {
    //         frame[WIDTH / CHAR_WIDTH * y + x] = 0xff;
    //     }
    // }
}

// :: TextRow graphics logic
// c is ascii byte of character
// idx is character index inside the text row.
void textrow_draw_unicode_point(uint8_t *textrow, uint32_t c, uint8_t idx) {
    // convert char into bitmap array index
    // if (c < 32) {
    //     c = 1;  // TODO: c = 0, but it's "!" for now
    // } else {
    //     c -= 32;
    // }

    int res;
    uint8_t glyph[16];
    res = get_full_glyph(c, glyph);
    if (res == 0) {
        get_full_glyph('!', glyph);
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        textrow[WIDTH / 8 * y + idx] = ~glyph[y];

        // uint8_t glyph[16];
        // memcpy_P(glyph, pgm_read_word(&(unicode_glyphs[c])), 16);
        // textrow[WIDTH / 8 * y + idx] =
        //     ~glyph[y];  // invert cuz char data uses black=clear
    }
}

void setup() {
    FATFS fs;
    FRESULT res;
    DIR dir;
    FILINFO fno;
    uint8_t buf[128];  // This holds bytes from the ebook
    UINT readcount;

    Serial.begin(9600);
    SPI.begin();

    // SD card chip select
    pinMode(5, OUTPUT);

    epd::init();
    epd::clear();
    // for (uint8_t i = 0; i < 12; i++) {
    //     textrow_draw_unicode_point(textrow, 'A', i);
    // }
    // epd::setPartialWindow(textrow, 0, 0, WIDTH, CHAR_HEIGHT);
    // epd::refreshDisplay();
    // while (1)
    //     ;

    disk_initialize();
    delay(100);

    res = pf_mount(&fs);
    if (res) {
        Serial.println(F("Failed to mount fs."));
        print_fresult(res);
    } else {
        Serial.println(F("MOUNTED!"));
    }

    res = pf_opendir(&dir, "/");
    if (res) {
        Serial.print(F("Failed to opendir. Error code: "));
        print_fresult(res);
    }

    // Read directory
    while (1) {
        res = pf_readdir(&dir, &fno);
        if (res != FR_OK) {
            Serial.println(F("Error at readdir."));
            print_fresult(res);
        }

        // end of listings
        if (!fno.fname[0]) {
            Serial.println(F("No ebook found."));
            while (true)
                ;
        }
        Serial.print(fno.fname);
        Serial.print("\t");
        Serial.println(fno.fsize);
        if (isTxtFile(&fno) && fno.fsize > 100) {
            break;
        }
    }

    Serial.println(F("Found ebook."));

    // Open file
    res = pf_open(fno.fname);
    if (res) {
        print_fresult(res);
        while (1)
            ;
    }

    res = pf_read(buf, sizeof(buf), &readcount);
    if (res) {
        print_fresult(res);
        while (1)
            ;
    }

    if (readcount != sizeof(buf)) {
        Serial.println(F("ebook ended prematurely"));
        while (1)
            ;
    }

    // Decode utf-8
    textrow_clear(textrow);
    auto *next = buf;  // our place in the buf
    long count = 0;
    uint8_t *end = buf + sizeof(buf) - 1;
    uint32_t cp;
    while (next < end) {
        next = utf8_simple(next, &cp);
        count++;
        Serial.println(cp);
        if (cp < 0) {
            Serial.println(F("cp was <0."));
            continue;
        }
        textrow_draw_unicode_point(textrow, cp, count - 1);
    }
    Serial.print("fptr is now: ");
    Serial.println(fs.fptr);
    epd::setPartialWindow(textrow, 0, 0, WIDTH, CHAR_HEIGHT);
    epd::refreshDisplay();

    // sprintf(buf, "%s", buf);
    // Serial.println(buf);
}

void loop() {
    // put your main code here, to run repeatedly:
}