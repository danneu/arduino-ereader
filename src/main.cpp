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
    // char buf[7 + 15];
    // sprintf(buf, "Error: %s", str[rc]);
    // Serial.println(buf);
}

bool isTxtFile(FILINFO *f) {
    // return f->fname[11] == 'T' && f->fname[10] == 'X' && f->fname[9] == 'T'
    // &&
    //        f->fname[8] == '.';  // && f->fattrib & AM_DIR == 0;
}

#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8
#define WIDTH 400
#define HEIGHT 300
#define CHARS_PER_ROW WIDTH / CHAR_WIDTH
#define ROWS_PER_PAGE HEIGHT / CHAR_HEIGHT
#define CHARS_PER_PAGE (CHARS_PER_ROW * ROWS_PER_PAGE)

#define TEXTROW_BUFSIZE CHAR_HEIGHT *WIDTH / CHAR_WIDTH
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
    uint8_t glyph[16];
    auto res = get_full_glyph(c, glyph);
    if (res == 0) {
        get_full_glyph('!', glyph);
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        textrow[WIDTH / 8 * y + idx] = ~glyph[y];
    }
}

PageResult draw_page(FATFS *fs, uint32_t offset, uint8_t *textrow) {
    PageResult p = {
        .bytesread = 0,
        .eof = false,
        .fres = FR_OK,
    };
    UINT readcount;
    FRESULT res;
    // uint8_t buf[64];                // holds bytes from ebook
    uint8_t buf[4];                 // holds bytes from ebook
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
                textrow_draw_unicode_point(textrow, ' ', x);
                continue;
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
                    buf[readcount] = '\0';
                }
            }

            // :: Decode next utf-8 and add it to row
            auto res = utf8_simple3(buf + bufidx, sizeof(buf) - bufidx);

            //  0 1 2 3 4
            // [_ _ _ o o]o
            //        ^
            //        EOF width=2
            if (res.evt == UTF8_EOF) {
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
        epd::setPartialWindow(textrow, 0, CHAR_HEIGHT * y, WIDTH, CHAR_HEIGHT);
    }
    epd::refreshDisplay();

    // Serial.print("BUfidx ended at: ");
    Serial.write(bufidx);

    p.fres = FR_OK;
    return p;
}

void setup() {
    FATFS fs;
    FRESULT res;
    DIR dir;
    FILINFO fno;
    // uint8_t buf[128];  // This holds bytes from the ebook
    UINT readcount;

    Serial.begin(9600);
    SPI.begin();

    // SD card chip select
    pinMode(5, OUTPUT);

    epd::init();
    epd::clear();

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
        Serial.print(F("\t"));
        Serial.print(fno.fext);
        Serial.print(F("\t"));
        Serial.println(fno.fsize);
        if (!strcmp(fno.fext, "TXT") && fno.fsize < 100) {
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

    // res = pf_read(buf, sizeof(buf), &readcount);
    // if (res) {
    //     print_fresult(res);
    //     while (1)
    //         ;
    // }

    // if (readcount != sizeof(buf)) {
    //     Serial.println(F("ebook ended prematurely"));
    //     while (1)
    //         ;
    // }

    PageResult p;
    auto byteloc = 0;

    while (1) {
        p = draw_page(&fs, fs.fptr - (fs.fptr - byteloc), textrow);
        if (p.fres != FR_OK) {
            Serial.println("draw_page retured bad FRESULT.");
            while (1)
                ;
        }
        byteloc += p.bytesread;
        delay(3000);
    }

    // Serial.print("fptr is now: ");
    // Serial.println(fs.fptr);
    // PageResult p = draw_page(&fs, fs.fptr, textrow);
    // if (p.fres != FR_OK) {
    //     // print_fresult(p.fres);
    //     while (true)
    //         ;
    // }
    // byteloc += p.bytesread;
    // Serial.print("PageResult.bytesdecoded: ");
    // Serial.println(p.bytesread);
    // Serial.print("fptr is now: ");
    // Serial.println(fs.fptr);

    // while (true)
    //     ;

    // // fptr = 640
    // // bytesdec = 582
    // delay(2000);
    // p = draw_page(&fs, fs.fptr - (fs.fptr - byteloc), textrow);
    // byteloc += p.bytesread;
    // Serial.print("PageResult.bytesdecoded: ");
    // Serial.println(p.bytesread);
    // Serial.print("fptr is now: ");
    // Serial.println(fs.fptr);

    // delay(2000);
    // p = draw_page(&fs, fs.fptr - (fs.fptr - byteloc), textrow);
    // byteloc += p.bytesread;
}

void loop() {
    // put your main code here, to run repeatedly:
}