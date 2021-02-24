#include <Arduino.h>

#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pff3a/source/diskio.h"
#include "pff3a/source/pff.h"
#include "spi.h"
#include "utf8.h"
#include "util.h"

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

uint8_t textrow[TEXTROW_BUFSIZE] = {0xff};

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
    // TODO: CLear buffer when skipping aroun book.
    uint8_t buf[64];
    uint8_t bufidx;
    // uint8_t *endptr;
    //
    // uint32_t pbuf[16];
    // uint8_t pidx;
} Struct;

State new_state(FATFS fs, uint32_t fsize) {
    State state = State{.fs = fs, .fsize = fsize, .buf = {0x00}, .bufidx = 64};
    return state;
}

typedef struct PTRESULT {
    UTF8_STATUS evt;
    bool eob;
    uint32_t pt;
    uint8_t width;
} PTRESULT;

UINT actual = 64;
// When bufidx is full, its index is 0
// Buf empty: idx 64-1

// REMEMBER: Don't udate sstuff in both next_codepoint and show_offset etc., lik
// don't update bufidx in diff places
PTRESULT next_codepoint(State *s) {
    PTRESULT p = PTRESULT{};
    p.eob = false;

    // BUf has 4 or fewer items left, fill it up

    // bufidx can be 0..63 (indexed) or 64 (empty)
    // the item it's pointing to is the item it wants you to take.
    // i.e. if bufidx is 63, it has that one item available before you can
    // finally inc it to 64.
    //
    // idx    len  (fn idx > 64-len)
    // 64     0    64>64-4 true
    // 63     1    63>60
    // 62     2
    // 61     3
    // 60     4    60>=60    true
    // 59     x    59>60    false
    if (64 - s->bufidx <= 4) {
        // if (s->endptr - s->buf > 4) {
        int len = 64 - s->bufidx;
        // EXPERIMENT HERE WITH -1 OR NOT. IDX=63 SHOULDNT HAVE LEN=1 right?????
        Serial.println("BUF almost empty");
        serial2(":: BUF almost empty. len =", len);
        delay(100);
        // serial3("len", s->bufidx, len);
        // move the bits to front of queue and splice in the rest from S
        // card

        // memcpy(&s->buf[0], &s->buf[s->bufidx], len);
        memcpy(&s->buf[0], &s->buf[s->bufidx], len);

        // if len=4, we want to skip 0, 1, 2, 3.
        // auto res = pf_read(s->buf + 4, 64, &actual);
        // auto res = pf_read(s->buf + len, 64 - len, &actual);

        // EDIT: I got it to go to the next page with this line.
        // I Should expiriment with taking out the len shit as its prob all
        // offbyone erors
        // auto res = pf_read(s->buf, 64, &actual);

        // I want to get this 4byte offset working tho
        auto res = pf_read(s->buf + len, 64 - len, &actual);

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
    // TODO: SHould I Lisetn to `actual` here?
    auto res = utf8_decode(s->buf + s->bufidx, actual - s->bufidx);
    if (true || res.evt != UTF8_OK) {
        serial5("UTF", res.evt, res.pt, res.width, s->buf[s->bufidx]);
    }
    if (res.evt == UTF8_EOI) {
        serial4("actual and actual-bufidx", actual, actual - s->bufidx,
                s->bufidx);
        serial1("TODO: Handle EOI.");
        while (1)
            ;
    }
    if (res.evt != UTF8_OK) {
        serial("error res", "");
    }

    if (res.evt == UTF8_OK || res.evt == UTF8_INVALID) {
        // Maybe try only += width on ok/invalid?
        // Wait, adding width doesn't make any sense lol. <-- NVM YES IT DOES
        // s->bufidx += 1;
    }
    s->bufidx += res.width;
    if (s->bufidx > 64) {
        serial1("wffffffffff");
        while (1)
            ;
    }

    p.evt = res.evt;
    p.pt = res.pt;
    p.width = res.width;
    return p;
}

bool pt_whitespace(uint32_t pt) {
    return pt == '\n' || pt == '\r' || pt == ' ';
}

#define ROWS_PER_PAGE (uint8_t)18  // 300/16
#define CHARS_PER_ROW (uint8_t)50  // 400/8

uint16_t show_offset(State *s, uint32_t offset, uint8_t *frame) {
    uint32_t pbuf[16];
    uint8_t pid = 0;
    uint8_t y = 0;
    uint16_t x = 0;
    // uint16_t x = 0;
    uint16_t consumed = 0;
    serial2("lseeking to:", offset);
    pf_lseek(offset);
    textrow_clear(frame);

// aka breakline()
// if (y >= ROWS_PER_PAGE - 1) {
//     serial1("hmm, called commitframe wheen I shouldnt");
//     goto exit;
// }
#define commitframe(y)                                             \
    do {                                                           \
        epd_set_partial_window(frame, 0, (y * CHAR_HEIGHT), WIDTH, \
                               CHAR_HEIGHT);                       \
        textrow_clear(frame);                                      \
        x = 0;                                                     \
        y++;                                                       \
    } while (0)

    while (y < ROWS_PER_PAGE) {
        // note: next_codepoint already incs bufidx

        // serial1("before next_codept");
        auto r = next_codepoint(s);
        // serial1("...affer next_codept");

        if (r.evt == UTF8_INVALID) {
            // We consume the byte offset, but we ignore the byte
            consumed += r.width;
        } else if (r.evt == UTF8_EOI) {
            // `next_codepoint` should be handling this for us.
            serial1("Unexpected UTF8_EOI in show_offset.");
        } else if (r.evt == UTF8_OK) {
            consumed += r.width;
            // We collect the point

            if (r.pt != '\n') {
                pbuf[pid++] = r.pt;
            }
            // pbuf[pid++] = r.pt;

            // \n is special case
            // TODO: Don't add it to pbuf
            if (r.pt == '\n') {
                commitframe(y);
                for (int i = 0; i < pid; i++) {
                    textrow_draw_unicode_point(textrow, pbuf[i], x++);
                }
                pid = 0;
                // if (x + pid < CHARS_PER_ROW) {
                //     for (int i = 0; i < pid; i++) {
                //         textrow_draw_unicode_point(frame, pbuf[i], x++);
                //     }
                //     pid = 0;  // reset the stck
                // } else {      // we donn't all fit on one row, so break
                // }

                // for (int i = 0; i < pid; i++) {
                //     if (x + 1 >= CHARS_PER_ROW) {
                //         ABORT("OOP");
                //     }
                //     textrow_draw_unicode_point(frame, pbuf[i], x++);
                // }
                // pid = 0;  // reset the stck
                // commitframe(y);
                // continue;
            } else if (pt_whitespace(r.pt)) {
                // spaces are a natural break point
                if (x + pid < CHARS_PER_ROW) {
                    for (int i = 0; i < pid; i++) {
                        textrow_draw_unicode_point(frame, pbuf[i], x++);
                    }
                    pid = 0;  // reset the stck
                } else {      // we donn't all fit on one row, so break
                    commitframe(y);
                    for (int i = 0; i < pid; i++) {
                        textrow_draw_unicode_point(textrow, pbuf[i], x++);
                    }
                    pid = 0;
                }
            } else if (pid >= 16) {
                serial1("pid >= 16");
                // buf is full so we're going to force-draw the codepoints
                // break-all style
                if (x + pid < CHARS_PER_ROW) {
                    serial1("x + pid < CHARSPERROW");
                    // all 16 fit on current line
                    for (int i = 0; i < pid; i++) {
                        textrow_draw_unicode_point(frame, pbuf[i], x++);
                    }
                    pid = 0;  // reset the stck
                } else {      // we donn't all fit on one row, so break
                    serial1("case33");
                    // not all 16 fit on curr line.
                    // TODO: Handle y-axis overflow or x-acis overflow/?
                    uint16_t currRow = min(pid, (uint16_t)CHARS_PER_ROW - x);
                    uint16_t nextRow = max(0, pid - currRow);
                    serial3("currRow vs nextRow:", currRow, nextRow);
                    if (currRow + nextRow != 16) {
                        serial1("check math lol");
                    }
                    // Handle current row
                    for (uint16_t i = 0; i < currRow; i++) {
                        // serial3("ps ", ps[i], i);
                        textrow_draw_unicode_point(textrow, pbuf[i], x++);
                    }
                    commitframe(y);
                    // Handle next row

                    for (uint16_t i = 0; i < nextRow; i++) {
                        textrow_draw_unicode_point(textrow, pbuf[i], x++);
                    }

                    // Reset the buffer
                    pid = 0;
                }
            } else {
                // TODO: Handle more cases
            }
        }
    }

exit:
    serial3("returning from how_ffset x y:", x, y);
    serial2("pid is: ", pid);

    epd_refresh();
    return consumed;
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
        show_offset(&state, fs.fptr, textrow);
    }
    // while (1) {
    //     show_offset(&state, loc, textrow);
    //     loc += 1;
    //     delay(200);
    // }
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
    // uint32_t byteloc = fs.fptr;  // inits to zero but also lets us seek
    // ahead

    // while (1) {
    //     // fs.fptr is always higher than byteloc because fptr has read
    //     more
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
