#include "book.h"

#include <Arduino.h>

#include "config.h"
#include "epd.h"
#include "glyphs.h"
#include "pixelbuf.h"
#include "utf8.h"
#include "util.h"

State new_state(FATFS fs, uint32_t fsize) {
    State state = State{.fs = fs, .fsize = fsize, .buf = {0x00}, .bufidx = 64};
    return state;
}

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
        Serial.println("BUF almost empty");
        serial2(":: BUF almost empty. len =", len);
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

        auto res = pf_read(s->buf + len, 64 - len, &actual);

        if (res != FR_OK) {
            Serial.println("prob");
        }
        if (actual < 64) {
            // end of book signalled
            serial2("pf_read actual<64:", actual);
            p.eob = true;
        }
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
    auto res = utf8_decode(s->buf + s->bufidx, actual - s->bufidx);
    p.evt = res.evt;
    p.pt = res.pt;
    p.width = res.width;

    if (res.evt != UTF8_OK) {
        serial5("UTF", res.evt, res.pt, res.width, s->buf[s->bufidx]);
    }
    if (res.evt == UTF8_EOI) {
        // serial4("actual and actual-bufidx", actual, actual - s->bufidx,
        // s->bufidx);
        // serial1("TODO: Handle EOI.");
        return p;
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

    return p;
}

bool pt_whitespace(uint32_t pt) {
    return pt == '\n' || pt == '\r' || pt == ' ';
}

////////////////////////////////////////////////////////////

uint16_t show_offset(State *s, uint32_t offset, pixelbuf *frame) {
    uint32_t pbuf[16];
    uint8_t pid = 0;
    uint8_t y = 0;
    uint16_t x = 0;
    bool eob = false;
    // uint16_t x = 0;
    uint16_t consumed = 0;
    serial2("lseeking to:", offset);
    pf_lseek(offset);
    pixelbuf_clear(frame);

// aka breakline()
// if (y >= ROWS_PER_PAGE - 1) {
//     serial1("hmm, called commitframe wheen I shouldnt");
//     goto exit;
// }
#define commitframe(y)                                                  \
    do {                                                                \
        epd_set_partial_window(frame->buf, 0, (y * CHAR_HEIGHT), WIDTH, \
                               CHAR_HEIGHT);                            \
        pixelbuf_clear(frame);                                          \
        x = 0;                                                          \
        y++;                                                            \
    } while (0)

    while (y < ROWS_PER_PAGE) {
        // note: next_codepoint already incs bufidx

        // serial1("before next_codept");
        auto r = next_codepoint(s);
        if (r.eob) {
            eob = true;
        }
        if (eob && r.evt == UTF8_EOI) {
            serial1("next_codepoint: EOB");
            // TODO: Do I need to also wrap and do that handlig??
            commitframe(y);
            break;
        }
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
                serial3("next_codepoint: found NL. x, pid:", x, pid);
                int i = 0;
                // Finish out the rest of current line
                // for (; i < pid && x + i < CHARS_PER_ROW; i++) {
                for (; i < pid && x < CHARS_PER_ROW; i++) {
                    serial3("A", x, pid);
                    pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                }
                commitframe(y);
                // Carry leftovers to next row
                for (; i < pid; i++) {
                    serial3("B", x, pid);
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
                serial2("whitespace found", pid);
                // spaces are a natural break point
                if (x + pid < CHARS_PER_ROW) {
                    serial4("whitespace: can fit on row. x, pid, x+pid:", x,
                            pid, x + pid);
                    for (int i = 0; i < pid; i++) {
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }
                    pid = 0;  // reset the stck
                } else {      // we donn't all fit on one row, so break
                    serial4("whitespace: CANNOT fit on row. x, pid, x+pid:", x,
                            pid, x + pid);
                    commitframe(y);
                    for (int i = 0; i < pid; i++) {
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
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
                        pixelbuf_draw_unicode_glyph(frame, pbuf[i], x++);
                    }
                    commitframe(y);
                    // Handle next row

                    for (uint16_t i = 0; i < nextRow; i++) {
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

    serial3("returning from how_ffset x y:", x, y);
    serial2("pid is: ", pid);

    epd_refresh();
    return consumed;
}