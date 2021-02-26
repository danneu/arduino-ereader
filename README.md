# Dan's Arduino E-Reader

My 4.2" e-ink reader built with C on the Arduino.

I impulse purchased an Arduino hoping to learn some C and hardware, and I figured an e-ink display might be fun to hack on. Sure enough, it was.

This is my progress so far. Go easy on the code, I've never written a line of C before.

## Features

-   Uses less than 2K SRAM, as that's all the Arduino Uno/Nano has.
-   Loads ebooks from SD card (just .txt files for now)
-   UTF-8 decoding.
-   Unicode support for some languages.
-   Wraps instead of breaking words.

## Goals

-   Stay fun.
-   Stay under 2K SRAM. Maybe 4K eventually with epub support.
-   Battery powered.
-   Epub support.
-   Hyphenated word wrapping.

## Kit so far

-   $5 Arduino Nano or $15 Arduino Uno
-   $35 — [4.2in e-ink display](https://www.amazon.com/4-2inch-Module-Communicating-Resolution-Controller/dp/B074NR1SW2)
-   $7.50 — [https://www.adafruit.com/product/254](MicroSD card breakout board)

## Technical

### Reading SD/FAT32 with small buffer

Loading Arduino's SD card library immediately eats over 700 bytes of global variable memory due to aggressive buffering. FAT32's read command responds with 512 byte chunks so it's easy to
allocate a 512 byte sized buffer, but that's unnecessary.

I use the [Petit FAT](http://elm-chan.org/fsw/ff/00index_p.html) library which doesn't buffer
data internally so that I can use my own buffer of arbitrary size.

Reading from SD card is slow enough to where you don't want to do it every byte, but not so slow that you need to avoid reads at all costs. I use a 64 byte buffer for now which is going to need at least 14 reads per e-ink page, but it's not a big deal. I'd rather use memory surplus to buffer more pixel data.

### Small pixel buffer

The easy solution for graphics is to map pixels to memory with a buffer sized `{Width}*{Height}*{nPixelStates}`. This way you can draw the UI with nice abstractions, in layers, and out of order. But that would take up half of my SRAM.

Instead I buffer one line of glyphs at a time and walk it down the screen until all rows of text have been submitted to the e-ink buffer with the partial-window-update command. I could get this down to buffering just one glyph at a time if I need to.

### Paginating forwards

UTF-8 is a multi-byte encoding meaning that a glyph may be represented with one to multiple bytes. That means you don't know how many bytes you'll need to read to draw the next page.

To make things more complicated, you always don't know where you will end the line since you need to buffer each word until you know that it will fit.

I have an incremental solution where I fetch bytes from the SD card and decode glyphs from those bytes until I fill up the page.

### Paginating backwards

I'm not sure of how to implement "go to previous page".

I think I can decode utf-8 backwards by marching backwards byte by byte until I hit start of sequence masks like `(byte & 0x80)`, `(byte & 0xe0)`, etc. That way I can get a sequence of unicode points going backwards, but that doesn't solve the problem.

I don't know which unicode point will be the start of the previous page due to the complexity of the line-breaking logic. At every single codepoint I consume backwards, I could generate a page from that point to see if I can fill one (doing this many times until I have success), but that's hard to do with such tiny buffers and I'm not sure if forward vs reverse will match each other.

It seems like too much complexity to take on right now since my rendering logic is still so naive that it's monospaced and doesn't support glyphs of variable width. So I have some more thinking/research to do.

My solution for now is to keep a small history buffer of disk offsets every time I `next_page()` so that I have limited but trivial `prev_page()` support.

This is a good opportunity to take a break from yolo hackwork to read how anything else in computing does pagination.

## Thoughts
