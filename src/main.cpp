#include <Arduino.h>

#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pff3a/source/diskio.h"
#include "pff3a/source/pff.h"
#include "spi.h"
#include "utf8.h"

#define WIDTH 400
#define HEIGHT 300
#define CHARS_PER_ROW WIDTH / CHAR_WIDTH
#define ROWS_PER_PAGE HEIGHT / CHAR_HEIGHT
#define CHARS_PER_PAGE (CHARS_PER_ROW * ROWS_PER_PAGE)

// With only 2K memory, I can't do the easy thing of holding a 1:1 pixel buffer.
// For now I'm buffering draws to the display with a `textrow` that's exactly
// one row of text tall and stretches across the whole display.
//
// I don't actually need the buffer, but the textrow also has
// the side-effect of clearing out spaces that I don't overwrite myself
// since textrow_clear() turns it back into 0xff pixels. I'd like to instead
// figure out how to get `set_pixel(c, x ,y)` working with epd.
#define TEXTROW_BUFSIZE (CHAR_HEIGHT * WIDTH / CHAR_WIDTH)
uint8_t textrow[TEXTROW_BUFSIZE] = {0xff};

void textrow_clear(uint8_t *frame) {
    for (uint16_t i = 0; i < TEXTROW_BUFSIZE; i++) {
        frame[i] = 0xff;
    }
}

// :: TextRow graphics logic
// pt is unicode code point.
// idx is character index inside the text row, 0 to CHARS_PER_ROW.
void textrow_draw_unicode_point(uint8_t *textrow, uint32_t pt, uint8_t idx) {
    uint8_t glyph[16];
    auto notfound = get_glyph(pt, glyph);
    if (notfound) {
        // silly fallback
        get_glyph('_', glyph);
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        textrow[WIDTH / 8 * y + idx] = ~glyph[y];
    }
}

// The problem I had with this is that since I wasn't calling it for
// every text tile, old glyphs from old draws were appearing.
// void set_glyph(uint32_t cp, uint16_t row, uint16_t col) {
//     uint8_t glyph[16];
//     auto res = get_glyph(cp, glyph);
//     if (res == 0) {
//         get_glyph('!', glyph);
//     }

//     // invert the colors
//     for (auto i = 0; i < 16; i++) {
//         glyph[i] = ~glyph[i];
//     }

//     epd_set_partial_window(glyph, col * CHAR_WIDTH, row * CHAR_HEIGHT,
//                            CHAR_WIDTH, CHAR_HEIGHT);
// }

// A bag of data that gets returned from draw_page().
struct PageResult {
    // bytesrealized is a count of how many bytes we read from the disk buffer.
    uint32_t bytesrealized;
    bool eof;
    FRESULT fres;
};

// The stuff in this function is so annoying to test and write that I'm in no
// rush to refactor this mess. I'll need to get a better abstraction together
// when I want to implement something like smart word wrapping.
PageResult draw_page(FATFS *fs, uint32_t offset, uint8_t *textrow) {
    PageResult p = {
        .bytesrealized = 0,
        .eof = false,
        .fres = FR_OK,
    };
    UINT readcount;
    uint8_t buf[64];                // holds bytes from ebook
    uint32_t bufidx = sizeof(buf);  // trigger initial load

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

            // load from book bytes if we need to
            if (bufidx >= sizeof(buf)) {
                p.fres = pf_read(buf, sizeof(buf), &readcount);
                if (p.fres != FR_OK) {
                    return p;
                }
                bufidx = 0;
                if (readcount < sizeof(buf)) {
                    p.eof = true;
                }
            }

            // :: Decode next utf-8 and add it to row
            // readcount is better than sizeof(buf) here because readcount will
            // handle unfilled pages (eof)
            auto res = utf8_decode(buf + bufidx, readcount - bufidx);

            //  0 1 2 3 4
            // [_ _ _ o o]o
            //        ^
            //        EOF width=2
            if (res.evt == UTF8_EOI) {
                if (p.eof) {
                    continue;
                }

                p.fres = pf_lseek(fs->fptr - res.width);
                if (p.fres != FR_OK) {
                    return p;
                }
                x--;
                bufidx = sizeof(buf);  // now force the pf_read branch above
                continue;
            }

            p.bytesrealized += res.width;
            bufidx += res.width;

            if (res.evt == UTF8_INVALID) {
                continue;
            }

            if (res.pt == '\n') {
                broke = true;
                break;
            }
            // don't carry whitespace
            if (x == 0 && (res.pt == ' ')) {
                x = -1;
                continue;
            }
            textrow_draw_unicode_point(textrow, res.pt, x);
        }
        epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH, CHAR_HEIGHT);
    }

    epd_refresh();
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

    epd_set_partial_window(frame, 0, HEIGHT / 2, WIDTH, CHAR_HEIGHT);
    epd_refresh();
}

void setup() {
    FATFS fs;
    FRESULT res;
    DIR dir;
    FILINFO fno;

    Serial.begin(9600);
    spi_begin();

    // SD card chip select
    pinMode(SD_CS_PIN, OUTPUT);

    epd_init();
    epd_clear();

    disk_initialize();
    delay(100);

    res = pf_mount(&fs);
    if (res) {
        Serial.println(F("Failed to mount fs."));
        render_fresult(res, textrow);
        while (1)
            ;
    }

    res = pf_opendir(&dir, "/");
    if (res) {
        render_fresult(res, textrow);
        while (1)
            ;
    }

    // Read directory
    while (1) {
        res = pf_readdir(&dir, &fno);
        if (res != FR_OK) {
            render_fresult(res, textrow);
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
        if (!strcmp(fno.fext, "TXT") && fno.fsize > 100) {
            break;
        }
    }

    // Open file
    res = pf_open(fno.fname);
    if (res) {
        render_fresult(res, textrow);
        while (1)
            ;
    }

    // SEEK AHEAD
    // pf_lseek(430606);
    // pf_lseek(430260);
    // pf_lseek(429377);
    pf_lseek(428840);

    PageResult p;
    // The highest byte offset that we've rendered so far.
    uint32_t byteloc = fs.fptr;  // inits to zero but also lets us seek ahead

    while (1) {
        // fs.fptr is always higher than byteloc because fptr has read more
        // bytes from the ebook than we've been able to render so far.
        // e.g. fptr has loaded 64 more bytes of the book but we only
        // had space for 12 more glyphs.
        p = draw_page(&fs, fs.fptr - (fs.fptr - byteloc), textrow);
        Serial.print("fs.ptr: ");
        Serial.println(fs.fptr);
        if (p.fres != FR_OK) {
            Serial.println(F("draw_page retured bad FRESULT."));
            render_fresult(p.fres, textrow);
            while (1)
                ;
        }
        if (p.eof) {
            Serial.println(F("end of book."));
            while (1)
                ;
        }
        byteloc += p.bytesrealized;
        delay(3000);
    }
}

void loop() {}
