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

#define serial(str, val) (Serial.print(str), Serial.println(val))
#define serial3(str, val, val2) \
    do {                        \
        Serial.print(str);      \
        Serial.print(val);      \
        Serial.print(" :: ");   \
        Serial.println(val2);   \
                                \
    } while (0)

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
        serial("glyph not foun for ", pt);
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

////////////////////////////////////////////////////////////
struct State {
    uint8_t buf[64];
    uint32_t bufidx;
    uint32_t ptbuf[16];
    uint8_t ptidx;
    uint32_t byteloc;
    FATFS *fs;
};

State new_state(FATFS *fs) {
    State x = State{};
    x.fs = fs;
    x.bufidx = sizeof(x.buf);
    return x;
}

////////////////////////////////////////////////////////////

typedef struct PTRESULT {
    UTF8_STATUS evt;
    bool eob;
    uint32_t pt;
    uint8_t width;
} PTRESULT;

PTRESULT next_codepoint(State *s) {
    PTRESULT p = PTRESULT{};
    p.eob = false;

redo:

    UINT readcount;
    if (s->bufidx >= sizeof(s->buf)) {
        auto res = pf_read(s->buf, sizeof(s->buf), &readcount);
        if (res != FR_OK) {
            serial("res was bad: ", res);
        }
        s->bufidx = 0;
        if (readcount < sizeof(s->buf)) {
            p.eob = true;
        }
    }

    // serial("readcount: ", readcount);
    // serial("readcount - bufidx; ", readcount - s->bufidx);
    auto res = utf8_decode(s->buf + s->bufidx, readcount - s->bufidx);

    if (res.evt == UTF8_EOI) {
        if (p.eob) {
            return p;
        } else {
            // serial("redo", 3);
            // pf_lseek(s->fs->fptr - res.width);
            // s->bufidx = sizeof(s->buf);  // now force the pf_read branch
            // above goto redo;
        }
    }

    if (res.evt != UTF8_OK) {
        serial("utf8 decode bad: ", res.evt);
    }
    // TODO: Handle bad cases
    s->bufidx += res.width;
    // s->byteloc += res.width;

    p.width = res.width;
    p.evt = res.evt;
    p.pt = res.pt;
    // serial("next_codepoint: ", p.pt);
    return p;
}

bool pt_isspace(uint32_t pt) { return pt == ' ' || pt == '\n' || pt == '\r'; }

int seek_space(const uint32_t *pts, uint16_t len) {
    uint16_t i = 0;
    while (i++ < len - 1) {
        if (pt_isspace(pts[i])) {
            return i;
        }
    }
    return -1;
}

uint8_t pt_width(uint32_t pt) {
    if (pt <= 0x007f) return 1;
    if (pt >= 0x0080 && pt <= 0x07ff) return 2;
    if (pt >= 0x800 && pt <= 0xffff) return 3;
    if (pt >= 0x010000 && pt <= 0x10ffff) return 4;
    return 0;
    // U+0000 to U+007F are (correctly) encoded with one byte
    // U+0080 to U+07FF are encoded with 2 bytes
    // U+0800 to U+FFFF are encoded with 3 bytes
    // U+010000 to U+10FFFF are encoded with 4 bytes
}

void show_page(State *s) {
    serial("show_page fptr: ", s->fs->fptr);
    serial("byteloc: ", s->byteloc);
    uint16_t x = 0, y = 0;
    uint32_t ps[16];
    int ips = 0;

    // Goto byteloc
    // if (fs->fptr != offset) {
    pf_lseek(s->byteloc);

    textrow_clear(textrow);
    while (y < ROWS_PER_PAGE) {
        auto r = next_codepoint(s);

        // if (r.evt == UTF8_OK || r.evt == UTF8_INVALID) {
        //     s->byteloc += r.width;
        // }

        if (r.evt == UTF8_INVALID) {
            continue;
        } else if (r.evt == UTF8_EOI) {
            serial("EOIIIIIIIIIIIIIIIII", r.width);
            // don't update byteloc, just seek here next time.

            // rewind unralized codepoints
            // while (ips) {
            //     int blen = pt_width(ps[ips--]);
            //     serial("blen ", blen);
            //     s->byteloc -= blen;
            // }

            // serial("Seeking to fptr offset: ", s->fs->fptr - r.width);
            // pf_lseek(s->fs->fptr - r.width);
            // continue;

            // break;
        }

        // Catch NL
        // if (r.pt == '\n') {
        //     epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH,
        //                            CHAR_HEIGHT);
        //     textrow_clear(textrow);
        //     y++;
        //     x = 0;
        //     continue;
        // }

        // ps[ips++] = r.pt;

        if (r.pt == '\n') {
            // s->byteloc += pt_width('\n');
        } else {
            ps[ips++] = r.pt;
        }

        if (r.pt == '\n') {
            for (int i = 0; i < ips; i++) {
                textrow_draw_unicode_point(textrow, ps[i], x);
                x++;
                s->byteloc += pt_width(ps[i]);
            }
            ips = 0;
            epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH,
                                   CHAR_HEIGHT);
            textrow_clear(textrow);
            x = 0;
            y++;
        } else if (pt_isspace(r.pt)) {
            // serial("isspace x=", x);
            // ips is distance
            if (x + ips < CHARS_PER_ROW) {
                // serial("2. is room. ips=", ips);
                for (int i = 0; i < ips; i++) {
                    textrow_draw_unicode_point(textrow, ps[i], x);
                    x++;
                    s->byteloc += pt_width(ps[i]);
                }
                ips = 0;
            } else {
                // serial("2. is  NOT room", "");
                // not enough space on this row
                epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH,
                                       CHAR_HEIGHT);
                textrow_clear(textrow);
                y++;
                x = 0;
                for (int i = 0; i < ips; i++) {
                    textrow_draw_unicode_point(textrow, ps[i], x);
                    s->byteloc += pt_width(ps[i]);
                    x++;
                }
                ips = 0;
            }
        } else if (ips >= 16 - 1) {
            // check if full
            if (x + ips < CHARS_PER_ROW) {
                // serial("2. is room. ips=", ips);
                for (int i = 0; i < ips; i++) {
                    textrow_draw_unicode_point(textrow, ps[i], x);
                    s->byteloc += pt_width(ps[i]);
                    x++;
                }
                ips = 0;
            } else {
                // serial("2. is  NOT room", "");
                // not enough space on this row
                epd_set_partial_window(textrow, 0, CHAR_HEIGHT * y, WIDTH,
                                       CHAR_HEIGHT);
                textrow_clear(textrow);
                y++;
                x = 0;
                for (int i = 0; i < ips; i++) {
                    textrow_draw_unicode_point(textrow, ps[i], x);
                    s->byteloc += pt_width(ps[i]);
                    x++;
                }
                ips = 0;
            }
        } else {
            // not space, and buf isn't full
        }
    }
    epd_refresh();

    // for (int i = 0; i < ips; i++) {
    //     Serial.println(ps[i]);
    // }
    return;

    textrow_clear(textrow);
    for (y = 0; y < ROWS_PER_PAGE; y++) {
        serial("YYYYYYY: ", y);
        textrow_clear(textrow);

        //
        // auto space = seek_space(ps, ips);
        // if (space > -1) {

        // }

        while (1) {
            // serial3("x: ", x, ips);
            if (ips >= 16 - 1) {
                for (int i = 0; i < 16; i++) {
                    if (x >= CHARS_PER_ROW - 1) {
                        // epd_set_partial_window(textrow, 0, y *
                        // CHAR_HEIGHT,
                        //                        WIDTH, CHAR_HEIGHT);
                        x = 0;
                        memcpy(ps, ps + i, 16 - i);
                        ips = 16 - ips;
                        goto endwhile;
                    } else {
                        textrow_draw_unicode_point(textrow, ps[i], x);
                        x++;
                        // serial3("uncidode: ", ps[i], x);
                    }
                    // serial3("drawing pt at ", i, ps[i]);
                }
                // for (int j = i; j < 16 - i; j++) {
                //     textrow_draw_unicode_point(textrow, ps[j], x++);
                // }
                ips = 0;
            }

            PTRESULT pr = next_codepoint(s);
            // if (pt_isspace(pr.pt)) {
            //     break;  // next y
            // }
            if (pr.evt == UTF8_INVALID) {
                continue;
            }
            ps[ips++] = pr.pt;
        }
    endwhile:
        // textrow_draw_unicode_point(textrow, pr.pt, x);
        epd_set_partial_window(textrow, 0, y * CHAR_HEIGHT, WIDTH, CHAR_HEIGHT);
    }
    epd_refresh();
}

void show_pageOLD0(State *s) {
    uint16_t x, y;
    uint32_t ps[16];
    int ips = 0;

    textrow_clear(textrow);
    for (y = 0; y < ROWS_PER_PAGE; y++) {
        serial("YYYYYYY: ", y);
        textrow_clear(textrow);

        //
        // auto space = seek_space(ps, ips);
        // if (space > -1) {

        // }

        while (1) {
            // serial3("x: ", x, ips);
            if (ips >= 16 - 1) {
                for (int i = 0; i < 16; i++) {
                    if (x >= CHARS_PER_ROW - 1) {
                        // epd_set_partial_window(textrow, 0, y *
                        // CHAR_HEIGHT,
                        //                        WIDTH, CHAR_HEIGHT);
                        x = 0;
                        memcpy(ps, ps + i, 16 - i);
                        ips = 16 - ips;
                        goto endwhile;
                    } else {
                        textrow_draw_unicode_point(textrow, ps[i], x);
                        x++;
                        // serial3("uncidode: ", ps[i], x);
                    }
                    // serial3("drawing pt at ", i, ps[i]);
                }
                // for (int j = i; j < 16 - i; j++) {
                //     textrow_draw_unicode_point(textrow, ps[j], x++);
                // }
                ips = 0;
            }

            PTRESULT pr = next_codepoint(s);
            // if (pt_isspace(pr.pt)) {
            //     break;  // next y
            // }
            if (pr.evt == UTF8_INVALID) {
                continue;
            }
            ps[ips++] = pr.pt;
        }
    endwhile:
        // textrow_draw_unicode_point(textrow, pr.pt, x);
        epd_set_partial_window(textrow, 0, y * CHAR_HEIGHT, WIDTH, CHAR_HEIGHT);
    }
    epd_refresh();
}

////////////////////////////////////////////////////////////

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
        if (!strcmp(fno.fext, "TXT") && fno.fsize > 100) {
            break;
        }
    }

    // Open file
    res = pf_open(fno.fname);
    if (res) {
        while (1)
            ;
    }

    State state = new_state(&fs);
    show_page(&state);
    serial("1", 2);
    delay(2000);
    serial("1", 2);
    show_page(&state);
    delay(2000);
    show_page(&state);
    while (1)
        ;

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
