#include <Arduino.h>

#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pff3a/source/diskio.h"
#include "pff3a/source/pff.h"
#include "spi.h"
#include "utf8.h"

#define serial1(a)         \
    do {                   \
        Serial.println(a); \
    } while (0)
#define serial(a, b)       \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.println(b); \
    } while (0)
#define serial2(a, b)      \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.println(b); \
    } while (0)
#define serial3(a, b, c)   \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.print(b);   \
        Serial.print(" "); \
        Serial.println(c); \
    } while (0)
////////////////////////////////////////////////////////////

// void sandbox() {
//     State state = new_state(&fs);
//     to_page(state, 0);
//     next_page(state);
//     prev_page(state);
// }
#define WIDTH 400
#define HEIGHT 300
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16
#define TEXTROW_BUFSIZE (CHAR_HEIGHT * WIDTH / CHAR_WIDTH)

const uint8_t textrow[TEXTROW_BUFSIZE] = {0xff};

void textrow_clear(uint8_t *frame) {
    for (uint16_t i = 0; i < TEXTROW_BUFSIZE; i++) {
        frame[i] = 0xff;
    }
}
void textrow_draw_unicode_point(uint8_t *textrow, uint32_t pt, uint8_t idx) {
    uint8_t glyph[16];
    auto notfound = get_glyph(pt, glyph);
    if (notfound) {
        // silly fallback
        serial("glyph not foun for ", pt);
        get_glyph('_', glyph);
    }
    for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
        textrow[WIDTH / 8 * y + idx] = ~glyph[y];
    }
}

////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////

typedef struct State {
    FATFS fs;
    // char fname[];
    uint32_t fsize;
    uint8_t buf[64];
    uint8_t bufidx;
    uint8_t *endptr;
} Struct;

State new_state(FATFS fs, uint32_t fsize) {
    State state = State{.fs = fs,
                        // .fname = fname,
                        .fsize = fsize,
                        .buf = {0x00},
                        .bufidx = 64 - 1};
    state.endptr = state.buf;
    return state;
}

typedef struct PTRESULT {
    UTF8_STATUS evt;
    bool eob;
    uint32_t pt;
    uint8_t width;
} PTRESULT;

// When bufidx is full, its index is 0
// Buf empty: idx 64-1
PTRESULT next_codepoint(State *s) {
    serial2("bidx", s->bufidx);
    PTRESULT p = PTRESULT{};
    p.eob = false;

    // BUf has 4 or fewer items left, fill it up
    if (s->bufidx > 64 - 1 - 4) {
        // if (s->endptr - s->buf > 4) {
        int len = 64 - s->bufidx;
        Serial.println("BUF almost empty");
        serial3("len", s->bufidx, len);
        UINT actual;
        // move the bits to front of queue and splice in the rest from S card
        for (int i = 0; i < len; i++) {
            // len=4, i= 0  1  2  3.
            //        x=60 61 62 63
            auto oldidx = i + 64 - len;
            s->buf[i] = s->buf[oldidx];
        }
        // if len=4, we want to skip 0, 1, 2, 3.
        auto res = pf_read(s->buf + 4, 64, &actual);
        serial2("buf[0]==buf[1]", s->buf[0] == s->buf[1]);
        if (res != FR_OK) {
            Serial.println("prob");
        }
        if (actual < 64) {
            // end of book signalled
            p.eob = true;
        }
        // s->endptr = s->buf + actual;
        // serial2("actual", actual);
        s->bufidx = 0;
    }

    // auto res = utf8_decode(s->buf, (s->endptr--) - s->buf);
    auto res = utf8_decode(s->buf + s->bufidx, 64 - s->bufidx);
    if (res.evt != UTF8_OK) {
        serial("error res", "");
    }
    s->bufidx += res.width;

    p.evt = res.evt;
    p.pt = res.pt;
    p.width = res.width;
    return p;
}

void show_offset(State *s, uint32_t offset, uint8_t *frame) {
    // uint8_t y = 0;
    // uint16_t x = 0;
    pf_lseek(offset);
    textrow_clear(frame);
    // do {
    // } while (y++ < ROWS_PER_PAGE)
    auto r = next_codepoint(s);
    switch (r.evt) {
        case UTF8_OK:
            // serial3("UTF8_OK", r.pt, (char)r.pt);
            serial2("UTF8_OK:", s->buf[s->bufidx]);
            break;
        case UTF8_INVALID:
            serial1("UTF8_INVALID");
            break;
        case UTF8_EOI:
            serial1("UTF8_EOI");
            break;

        default:
            break;
    }
    // if (r.evt == UTF8_OK) Serial.println(r.width);
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

    // epd_init();
    // epd_clear();

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
            break;
        }
    }

    // Open file
    res = pf_open(fno.fname);
    if (res) {
        while (1)
            ;
    }

    // pf_lseek(428840);
    State state = new_state(fs, fno.fsize);
    auto loc = 0;
    while (1) {
        show_offset(&state, loc, textrow);
        loc += 1;
        // delay(200);
    }
    // serial("1", 2);
    // delay(2000);
    // serial("1", 2);
    // show_page(&state);
    // delay(2000);
    // show_page(&state);
    while (1)
        ;

    // SEEK AHEAD
    // pf_lseek(430606);
    // pf_lseek(430260);
    // pf_lseek(429377);
    // pf_lseek(428840);

    // PageResult p;
    // // The highest byte offset that we've rendered so far.
    // uint32_t byteloc = fs.fptr;  // inits to zero but also lets us seek ahead

    // while (1) {
    //     // fs.fptr is always higher than byteloc because fptr has read more
    //     // bytes from the ebook than we've been able to render so far.
    //     // e.g. fptr has loaded 64 more bytes of the book but we only
    //     // had space for 12 more glyphs.
    //     p = draw_page(&fs, fs.fptr - (fs.fptr - byteloc), textrow);
    //     Serial.print("fs.ptr: ");
    //     Serial.println(fs.fptr);
    //     if (p.fres != FR_OK) {
    //         Serial.println(F("draw_page retured bad FRESULT."));
    //         while (1)
    //             ;
    //     }
    //     if (p.eof) {
    //         Serial.println(F("end of book."));
    //         while (1)
    //             ;
    //     }
    //     byteloc += p.bytesrealized;
    //     delay(3000);
    // }
}

void loop() {}
