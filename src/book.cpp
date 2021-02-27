#include "book.h"

#include <Arduino.h>

#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pixelbuf.h"
#include "utf8.h"
#include "util.h"

State new_state(FATFS *fs, uint32_t fsize) {
    State state = State{.fs = fs,
                        .fsize = fsize,
                        .buf = {0x00},
                        .bufidx = SD_BUFSIZE,
                        .buflen = 0,
                        .history = {0x00},
                        .hid = -1};
    return state;
}

void _abort_helper(char (*funct)(const char *), const char *string,
                   pixelbuf *frame) {
    char buf[CHARS_PER_ROW];
    uint8_t cursor = 0;

    while ((buf[cursor] = funct(string++))) {
        ++cursor;
    }
    for (uint8_t y = 0; y < ROWS_PER_PAGE; y++) {
        pixelbuf_clear(frame);
        if (y == ROWS_PER_PAGE / 2 - 1) {
            uint8_t msg[6];
            memcpy_P(msg, PSTR("Error:"), sizeof(msg));
            for (uint8_t i = 0; i < sizeof(msg); i++) {
                pixelbuf_draw_unicode_glyph(
                    frame, msg[i], i + (CHARS_PER_ROW / 2) - (sizeof(msg) / 2));
            }
        } else if (y == ROWS_PER_PAGE / 2) {
            for (uint8_t i = 0; i < cursor; i++) {
                pixelbuf_draw_unicode_glyph(
                    frame, buf[i], i + (CHARS_PER_ROW / 2) - (cursor / 2));
            }
        }
        epd_set_partial_window(frame->buf, 0, y * CHAR_HEIGHT, WIDTH,
                               CHAR_HEIGHT);
    }
    epd_refresh();
    while (1)
        ;
}

void abort_with_error(const char *line, pixelbuf *frame) {
    _abort_helper([](const char *ptr) { return *ptr; }, line, frame);
}

void abort_with_error(const __FlashStringHelper *line, pixelbuf *frame) {
    _abort_helper([](const char *ptr) { return (char)pgm_read_byte(ptr); },
                  (const char *)line, frame);
}

const char ferrors[] PROGMEM =
    "(No Error)   Disk Error   No File      Not Opened   Not Enabled  No "
    "Filesystem";

// Render FRESULT to screen and wait for user to reset (likely missing SD card).
void abort_with_ferror(FRESULT res, pixelbuf *frame) {
    char msg[14];
    memcpy_P(msg, &ferrors[res * 13], 13);
    msg[13] = 0;
    for (uint8_t y = 0; y < ROWS_PER_PAGE; y++) {
        pixelbuf_clear(frame);
        if (y == (ROWS_PER_PAGE / 2) - 1) {
            char msg[15];
            strcpy_P(msg, PSTR("SD Card Error:"));
            // auto msg = F("SD Card Error:");
            for (uint8_t i = 0; i < sizeof(msg) - 1; i++) {
                pixelbuf_draw_unicode_glyph(
                    frame, msg[i],
                    i + (CHARS_PER_ROW / 2) - ((sizeof(msg) - 1) / 2));
            }
        } else if (y == (ROWS_PER_PAGE / 2)) {
            for (uint8_t i = 0; i < 14 - 1; i++) {
                pixelbuf_draw_unicode_glyph(frame, msg[i],
                                            i + (CHARS_PER_ROW / 2) - (14 / 2));
            }
        }
        epd_set_partial_window(frame->buf, 0, y * CHAR_HEIGHT, WIDTH,
                               CHAR_HEIGHT);
    }
    epd_refresh();
    while (1)
        ;
}

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
    // serial2("next_codepoint: bufidx is", s->bufidx);
    if (SD_BUFSIZE - s->bufidx <= 4) {
        // if (s->endptr - s->buf > 4) {
        int len = SD_BUFSIZE - s->bufidx;
        // serial2(":: BUF almost empty. len =", len);
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
        // auto res = pf_read(s->buf + len, 64 - len, &actual);
        // serial2("-> actual", actual);

        auto res = pf_read(s->buf + len, SD_BUFSIZE - len, &s->buflen);
        // add back the bytes at the front (memcpy step above)
        s->buflen += len;

        if (res != FR_OK) {
            // TODO
            Serial.println(F("TODO: Handle utf-8 issue."));
        }
        // if (actual < 64) {
        //     // end of book signalled
        //     serial2("pf_read actual<64:", actual);
        //     p.eob = true;
        // }
        // s->endptr = s->buf + actual;
        // serial2("actual", actual);
        s->bufidx = 0;
    }

    // auto res = utf8_decode(s->buf, (s->endptr--) - s->buf);
    // TODO: SHould I Lisetn to `actual` here?
    // Note: Ignoring actual for now cuz it's always 60(?)
    // Note: Ignoring actual for now cuz it's always 60(?)
    // auto res = utf8_decode(s->buf + s->bufidx, actual - s->bufidx);
    // auto res = utf8_decode(s->buf + s->bufidx, 64 - s->bufidx);
    // if p.eob == 1 and actual-s->bufidx == 0 then we're eob.
    auto res = utf8_decode(s->buf + s->bufidx, s->buflen - s->bufidx);
    p.evt = res.evt;
    p.pt = res.pt;
    p.width = res.width;

    if (res.evt != UTF8_OK) {
        serial5(F("UTF"), res.evt, res.pt, res.width, s->buf[s->bufidx]);
    }
    if (res.evt == UTF8_EOI) {
        // serial4("actual and actual-bufidx", actual, actual - s->bufidx,
        // s->bufidx);
        // serial1("TODO: Handle EOI.");
        return p;
    }

    if (res.evt == UTF8_OK || res.evt == UTF8_INVALID) {
        // Maybe try only += width on ok/invalid?
        // Wait, adding width doesn't make any sense lol. <-- NVM YES IT DOES
        // s->bufidx += 1;
    }
    s->bufidx += res.width;
    if (s->bufidx > SD_BUFSIZE) {
        while (1)
            ;
    }

    return p;
}

bool pt_whitespace(uint32_t pt) {
    return pt == '\n' || pt == '\r' || pt == ' ';
}

////////////////////////////////////////////////////////////

uint16_t show_offset(State *s, uint32_t offset, pixelbuf *frame) {
    if (offset >= s->fs->fsize) {
        serial1(F("Not paginating because offset==fsize. Bailing..."));
        return 0;
    }
    serial2(F("lseeking to:"), offset);
    pf_lseek(offset);
    uint32_t start_fptr = s->fs->fptr;  // do this afer the lseek

    uint32_t pbuf[16];
    uint8_t pid = 0;
    uint8_t y = 0;
    uint16_t x = 0;
    // bool eob = false;
    // uint16_t x = 0;
    pixelbuf_clear(frame);
    // Gives us idx of the last pid we rendered so we can calculate
    // a rewind at the end.
    uint8_t pidoffset = 0;

// aka breakline()
// if (y >= ROWS_PER_PAGE - 1) {
//     serial1("hmm, called commitframe wheen I shouldnt");
//     goto exit;
// }
#define commitframe(y, _pidoffset)                                      \
    do {                                                                \
        pidoffset = _pidoffset;                                         \
        epd_set_partial_window(frame->buf, 0, (y * CHAR_HEIGHT), WIDTH, \
                               CHAR_HEIGHT);                            \
        pixelbuf_clear(frame);                                          \
        x = 0;                                                          \
        y++;                                                            \
        if (y >= ROWS_PER_PAGE) {                                       \
            goto bail;                                                  \
        }                                                               \
    } while (0)

    while (y < ROWS_PER_PAGE) {
        pidoffset = 0;
        // serial2("-- y:", y);
        // note: next_codepoint already incs bufidx

        // serial1("before next_codept");
        auto r = next_codepoint(s);
        // if (r.eob) {
        //     eob = true;
        // }
        // serial1("...affer next_codept");

        if (r.evt == UTF8_INVALID) {
            // We consume the byte offset, but we ignore the byte
        } else if (r.evt == UTF8_EOI) {
            // `next_codepoint` should be handling this for us.
            serial1(F("Unexpected UTF8_EOI in show_offset."));
        } else if (r.evt == UTF8_OK) {
            // We collect the point

            // if (r.pt != '\n') {
            //     pbuf[pid++] = r.pt;
            // }
            pbuf[pid++] = r.pt;

            if (s->buflen < SD_BUFSIZE && s->bufidx >= s->buflen) {
                // serial1("next_codepoint: EOB");
                // TODO: Do I need to also wrap and do that handlig??
                serial2(F("EOB... pid is"), pid);
                // TODO: Hadle newlines/line dont fit
                for (int i = 0; i < pid; i++) {
                    pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                }
                pid = 0;
                commitframe(y, 0);
                break;
            }

            // \n is special case
            if (r.pt == '\n') {
                // serial3("next_codepoint: found NL. x, pid:", x, pid);
                int i = 0;

                // Ignore leading NL on a page
                // FIXME: Sloppy
                // if (pid == 1 && y == 0) {
                //     y -= 1;
                //     x = 0;
                //     pid = 0;
                //     continue;
                // }

                if (x + pid >= CHARS_PER_ROW) {
                    commitframe(y, 0);
                }

                // Finish out the rest of current line
                // for (; i < pid && x + i < CHARS_PER_ROW; i++) {
                for (; i < pid && x < CHARS_PER_ROW; i++) {
                    // serial3("A", x, pid);
                    pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                }
                commitframe(y, i);
                // Carry leftovers to next row
                for (; i < pid; i++) {
                    // serial3("B", x, pid);
                    pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                }
                pid = 0;

                // for (int i = 0; i < pid; i++) {
                //     textrow_draw_unicode_point(textrow, pbuf[i], x++);
                // }
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
                // serial2("whitespace found", pid);
                // spaces are a natural break point
                if (x + pid < CHARS_PER_ROW) {
                    // serial4("whitespace: can fit on row. x, pid, x+pid:", x,
                    // pid, x + pid);
                    for (int i = 0; i < pid; i++) {
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }
                    pid = 0;  // reset the stck
                } else {      // we donn't all fit on one row, so break
                    // serial4("whitespace: CANNOT fit on row. x, pid, x+pid:",
                    // x, pid, x + pid);
                    commitframe(y, 0);
                    for (int i = 0; i < pid; i++) {
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }
                    pid = 0;
                }
            } else if (pid >= 16) {
                // buf is full so we're going to force-draw the codepoints
                // break-all style
                if (x + pid < CHARS_PER_ROW) {
                    // all 16 fit on current line
                    for (uint8_t i = 0; i < pid; i++) {
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }
                    pid = 0;  // reset the stck
                } else {      // we donn't all fit on one row, so break
                    // int i = 0;
                    // // Finish out the rest of current line
                    // for (; i < pid && x + i < CHARS_PER_ROW; i++) {
                    //     textrow_draw_unicode_point(frame, pbuf[i], x++);
                    // }
                    // commitframe(y);
                    // // Carry leftovers to next row
                    // for (; i < pid; i++) {
                    //     textrow_draw_unicode_point(frame, pbuf[i], x++);
                    // }
                    // pid = 0;

                    ///////
                    // not all 16 fit on curr line.
                    // TODO: Handle y-axis overflow or x-acis overflow/?
                    uint16_t currRow = min(pid, (uint16_t)CHARS_PER_ROW - x);
                    uint16_t nextRow = max(0, pid - currRow);
                    // serial3("currRow vs nextRow:", currRow, nextRow);
                    if (currRow + nextRow != 16) {
                        serial1(F("check math lol"));
                    }
                    // Handle current row
                    uint8_t i;
                    for (i = 0; i < currRow; i++) {
                        // serial3("ps ", ps[i], i);
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }
                    commitframe(y, i);
                    // Handle next row

                    for (; i < nextRow; i++) {
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }

                    // Reset the buffer
                    pid = 0;
                }
            } else {
                // TODO: Handle more cases
            }
        }
    }

bail:

    int bytesize = 0;
    for (int i = pidoffset; i < pid; i++) {
        bytesize += utf8_encoded_bytesize(pbuf[i]);
    }
    auto actual_consumed = s->fs->fptr - start_fptr;
    auto offset1 = actual_consumed
                   // Unused decoded points
                   - bytesize
                   // Unused read buffer
                   - (s->buflen - s->bufidx);

    // Reset bufidx
    s->bufidx = SD_BUFSIZE;

    pixelbuf_clear(frame);
    pixelbuf_draw_progress(frame, (double)start_fptr / s->fsize);
    epd_set_partial_window(frame->buf, 0, (ROWS_PER_PAGE)*CHAR_HEIGHT, WIDTH,
                           CHAR_HEIGHT);

    // epd_set_lut(LUT_FULL);
    epd_refresh();

    return offset1;
}

////////////////////////////////////////////////////////////
// Main API

void book_goto_beginning(State *s, pixelbuf *frame) {
    auto res = pf_lseek(0);
    if (res) abort_with_ferror(res, frame);
    return book_next_page(s, frame);
}

void book_next_page(State *s, pixelbuf *frame) {
    auto start = s->fs->fptr;
    auto diff = show_offset(s, start, frame);
    auto res = pf_lseek(start + diff);
    if (res) abort_with_ferror(res, frame);

    // Push to history
    if (s->hid >= PAGE_HISTORY_LEN - 1) {
        // TODO: Haven't checked this branch
        for (int i = 0; i < PAGE_HISTORY_LEN - 1; i++) {
            s->history[i] = s->history[i + 1];
        }
        s->history[PAGE_HISTORY_LEN - 1] = start;
    } else {
        s->history[++s->hid] = start;
    }

    // return diff;
}

void book_prev_page(State *s, pixelbuf *frame) {
    if (s->hid - 1 < 0) {
        return;
    } else {
        auto start = s->history[--s->hid];
        auto diff = show_offset(s, start, frame);
        auto res = pf_lseek(start + diff);
        if (res) abort_with_ferror(res, frame);
    }
}

uint32_t book_getoffset(State *s) { return s->fs->fptr; }