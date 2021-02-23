#include <Arduino.h>
// #include <SPI.h>
#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pff3a/source/diskio.h"
#include "pff3a/source/pff.h"
#include "spi.h"
#include "utf8.h"

// TODO: http://elm-chan.org/docs/mmc/mmc_e.html
// Impl examples: https://gist.github.com/RickKimball/2325039

static void print_fresult(FRESULT rc) {
    static const char str[][15] = {
        "OK            ", "DISK_ERR      ", "NOT_READY     ", "NO_FILE       ",
        "NO_PATH       ", "NOT_OPENED    ", "NOT_ENABLED   ", "NO_FILE_SYSTEM"};
    // char buf[7 + 15];
    // sprintf(buf, "Error: %s", str[rc]);
    // Serial.println(buf);
}

#define WIDTH 400
#define HEIGHT 300
#define CHARS_PER_ROW WIDTH / CHAR_WIDTH
#define ROWS_PER_PAGE HEIGHT / CHAR_HEIGHT
#define CHARS_PER_PAGE (CHARS_PER_ROW * ROWS_PER_PAGE)

#define TEXTROW_BUFSIZE (CHAR_HEIGHT * WIDTH / CHAR_WIDTH)
uint8_t textrow[TEXTROW_BUFSIZE] = {0xff};

struct PageResult {
    uint16_t bytesread;
    bool eof;
    FRESULT fres;
};

void textrow_clear(uint8_t *frame) {
    for (uint16_t i = 0; i < TEXTROW_BUFSIZE; i++) {
        frame[i] = 0xff;
    }
}

// :: TextRow graphics logic
// c is ascii byte of character
// idx is character index inside the text row.
void textrow_draw_unicode_point(uint8_t *textrow, uint32_t c, uint8_t idx) {
    uint8_t glyph[16];
    auto res = get_glyph(c, glyph);
    if (res) {
        get_glyph('_', glyph);
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        textrow[WIDTH / 8 * y + idx] = ~glyph[y];
    }
}

void set_glyph(uint32_t cp, uint16_t row, uint16_t col) {
    uint8_t glyph[16];
    auto res = get_glyph(cp, glyph);
    if (res == 0) {
        get_glyph('!', glyph);
    }

    // invert the colors
    for (auto i = 0; i < 16; i++) {
        glyph[i] = ~glyph[i];
    }

    epd_set_partial_window(glyph, col * CHAR_WIDTH, row * CHAR_HEIGHT,
                           CHAR_WIDTH, CHAR_HEIGHT);
}

PageResult draw_page(FATFS *fs, uint32_t offset, uint8_t *textrow) {
    PageResult p = {
        .bytesread = 0,
        .eof = false,
        .fres = FR_OK,
    };
    UINT readcount;
    FRESULT res;
    uint8_t buf[64];  // holds bytes from ebook
    // uint8_t buf[4];                 // holds bytes from ebook
    uint32_t bufidx = sizeof(buf);  // trigger initial load
    // uint32_t cp;                    // decoded code point
    // uint8_t width;  // utf8 bytes consumed to produce the current unicode
    // codepoint.

    if (fs->fptr != offset) {
        p.fres = pf_lseek(offset);
        if (p.fres != FR_OK) {
            return p;
        }
    }

    bool broke = false;
    for (int y = 0; y < ROWS_PER_PAGE; y++) {
        textrow_clear(textrow);

        for (int x = 0; x < CHARS_PER_ROW; x++) {
            if (broke && x < 4) {
                // textrow_draw_unicode_point(textrow, ' ', x);
                // continue;
            }
            broke = false;

            // broke = false;
            // load from book bytes if we need to
            if (bufidx >= sizeof(buf)) {
                p.fres = pf_read(buf, sizeof(buf), &readcount);
                if (p.fres != FR_OK) {
                    return p;
                }
                bufidx = 0;
                if (readcount < sizeof(buf)) {
                    // bookover = true;
                    p.eof = true;
                }
            }

            // :: Decode next utf-8 and add it to row
            // readcount is better than sizeof(buf) here because readcount will
            // handle unfilled pages (eof)
            auto res = utf8_simple3(buf + bufidx, readcount - bufidx);
            // Serial.println(res.cp);

            //  0 1 2 3 4
            // [_ _ _ o o]o
            //        ^
            //        EOF width=2
            if (res.evt == UTF8_EOF) {
                if (p.eof) {
                    // make sure we paint our progress
                    epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH,
                                           CHAR_HEIGHT);
                    goto exit;
                }

                p.fres = pf_lseek(fs->fptr - res.width);
                if (p.fres != FR_OK) {
                    return p;
                }
                x--;
                bufidx = sizeof(buf);  // now force the pf_read branch above
                continue;
            }

            p.bytesread += res.width;
            bufidx += res.width;

            if (res.evt == UTF8_INVALID) {
                continue;
            }

            if (res.cp == '\n') {
                broke = true;
                break;
            }
            // don't carry whitespace
            if (x == 0 && (res.cp == ' ')) {
                x = -1;
                continue;
            }
            textrow_draw_unicode_point(textrow, res.cp, x);
        }
        epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH, CHAR_HEIGHT);
    }
exit:

    epd_refresh();

    // Serial.print("BUfidx ended at: ");
    // Serial.write(bufidx);

    // p.fres = FR_OK;
    return p;
}

static void render_fresult(FRESULT rc, uint8_t *frame) {
    static const char str[][15] = {
        "OK            ", "DISK_ERR      ", "NOT_READY     ", "NO_FILE       ",
        "NO_PATH       ", "NOT_OPENED    ", "NOT_ENABLED   ", "NO_FILE_SYSTEM"};
    const char *msg = str[rc];
    textrow_clear(frame);
    for (uint8_t i = 0; msg[i] != 0; i++) {
        textrow_draw_unicode_point(frame, msg[i],
                                   WIDTH / CHAR_WIDTH / 2 - 15 / 2 + i);
    }

    epd_set_partial_window(frame, 0, 300 / 2, 400, CHAR_HEIGHT);
    epd_refresh();
}

void setup() {
    FATFS fs;
    FRESULT res;
    DIR dir;
    FILINFO fno;
    // uint8_t buf[128];  // This holds bytes from the ebook
    UINT readcount;

    Serial.begin(9600);
    spi_begin();

    // SD card chip select
    pinMode(SD_CS_PIN, OUTPUT);

    epd_init();
    epd_clear();

    // while (1)
    //     ;

    disk_initialize();
    delay(100);

    render_fresult(FR_NO_FILE, textrow);
    delay(5000);

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
        Serial.print(F("\t"));
        Serial.print(fno.fext);
        Serial.print(F("\t"));
        Serial.println(fno.fsize);
        if (!strcmp(fno.fext, "TXT") && fno.fsize > 100) {
            break;
        }
    }

    Serial.println(F("Found ebook."));
    // while (true)
    //     ;

    // Open file
    res = pf_open(fno.fname);
    if (res) {
        print_fresult(res);
        while (1)
            ;
    }

    PageResult p;
    auto byteloc = 0;

    while (1) {
        p = draw_page(&fs, fs.fptr - (fs.fptr - byteloc), textrow);
        if (p.fres != FR_OK) {
            Serial.println("draw_page retured bad FRESULT.");
            while (1)
                ;
        }
        if (p.eof) {
            Serial.println("end of book.");
            while (1)
                ;
        }
        byteloc += p.bytesread;
        delay(3000);
    }
}

void loop() {
    // put your main code here, to run repeatedly:
}
