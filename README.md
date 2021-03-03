# Dan's Arduino E-Reader

My 4.2" e-ink reader built with C on the Arduino.

I impulse purchased an Arduino hoping to learn some C and hardware, and I figured an e-ink display might be fun to hack on. Sure enough, it was.

This is my progress so far. Go easy on the code, I've never written a line of C before.

## Features

-   Uses less than 2K SRAM / 32K flash as that's all the ATmega328P has
-   Loads ebooks from SD card
-   UTF-8 decoding
-   Unicode support for some languages
-   Wraps instead of breaking words

## Goals

-   Stay fun
-   Stay under 2K SRAM, maybe 4K eventually
-   Battery powered
-   Hyphenated word wrapping

## Kit so far

-   $5 Arduino Nano or $15 Arduino Uno
-   $35 — [4.2in e-ink display](https://www.amazon.com/4-2inch-Module-Communicating-Resolution-Controller/dp/B074NR1SW2)
-   $7.50 — [https://www.adafruit.com/product/254](MicroSD card breakout board)

## Usage

This project assumes that `.txt` files in the SD card root directory are ebooks.

Ebooks need to stripped of html and converted into a simple plaintext format.

You can do this with tools like [pandoc](https://pandoc.org/) `pandoc book.epub -o book.txt` or any of the online tools that can do this for you: https://www.zamzar.com/convert/epub-to-txt/

## Technical

### Reading SD/FAT32 with small buffer

Loading Arduino's SD card library immediately eats over 700 bytes of global variable memory due to aggressive buffering. FAT32's read command responds with 512 byte chunks so it's easy to
allocate a 512 byte sized buffer, but that's unnecessary.

I use the [Petit FAT](http://elm-chan.org/fsw/ff/00index_p.html) library which doesn't buffer
data internally so that I can use my own buffer of arbitrary size.

Reading from SD card is slow enough to where you don't want to do it every byte, but not so slow that you need to avoid reads at all costs. I use a 64 byte buffer for now which is going to need at least 14 reads per e-ink page, but it's not a big deal.

### Small pixel buffer

The easy solution for graphics is to map pixels to memory 1:1 with a buffer sized `{Width}*{Height}*{nPixelStates}`. This way you can draw the UI with nice abstractions, in layers, and out of order. But that would take up half of my SRAM.

Instead I buffer one line of glyphs at a time and walk it down the screen until all rows of text have been submitted to the e-ink buffer with the partial-window-update command. I could get this down to buffering just one glyph at a time if I need to.

### Paginating forwards

UTF-8 is a multi-byte encoding meaning that a glyph may be represented with one to multiple bytes. That means you don't know how many bytes you'll need to read to draw the next page.

To make things more complicated, you always don't know where you will end the line since you need to buffer each word until you know that it will fit.

I have an incremental solution where I fetch bytes from the SD card and decode glyphs from those bytes until I fill up the page.

### Paginating backwards

I'm not sure how to implement "go to previous page".

I think I can decode utf-8 backwards by marching backwards byte by byte until I hit start of sequence masks like `(byte & 0x80)`, `(byte & 0xe0)`, etc. That way I can get a sequence of unicode points going backwards, but that doesn't solve the problem.

I don't know which unicode point will be the start of the previous page due to the complexity of the line-breaking logic. At every single codepoint I consume backwards, I could generate a page from that point to see if I can fill one (doing this many times until I have success), but that's hard to do with such tiny buffers and I'm not sure if forward vs reverse will match each other.

It seems like too much complexity to take on right now since my rendering logic is still so naive that it's monospaced and doesn't support glyphs of variable width. So I have some more thinking/research to do.

My solution for now is to keep a small history buffer of disk offsets every time I `next_page()` so that I have limited but trivial `prev_page()` support.

This is a good opportunity to take a break from yolo hackwork to read how anything else in computing does pagination.

### Tracking ebook position

Any ebook reader of course should be able to keep track of where you are in each book.

I currently have not implemented a solution.

One solution is to store this information in the ebook file over SD which would be nice.

FAT32 requires that I write 512 sector-aligned bytes at a time. One simple solution here is to require that
the ebook file starts with a 512 byte area and then the ebook begins on the 513th byte. That way I don't need to do any buffering.

For example, if I just asked for ebooks to begin after 4 bytes (giving me a place to store a uint32 number of the user's place in the book), then I'd have to buffer 512 bytes on every write so that I could replay 508 bytes of content every time I update that 4-byte number. That's 1/4th of my SRAM and makes it easy to OOM, so that's something I'd like to avoid for now.

### Unicode

I use a subset of [GNU Unifont](http://unifoundry.com/unifont/) glyphs.

I only have 32K of flash storage on the chip to hold such data, so I had to exclude most of unicode to fit a small subset on the chip.

Supporting all languages and all of unicode would require more storage, though I could come up with a compression solution to get more mileage out of 32K flash.

While I want to keep this project as simple as possible for now, one solution is to fit another chip on board that can hold all of Unifont's glyphs and allow me glyph lookups over SPI.

### TXT vs EPUB

I require ebooks to be in a simple text format where all glyphs in the book are going to be rendered on the display.

```
pandoc book.epub -o book.txt
```

EPUB2 support would take a decent amount of work and substantially complicate the code. For example, EPUB is basically a zip archive of html documents. It's easy to handle this with lots of memory, but not so obvious with just 2K SRAM.

### Button interface

I currently use the single button provided by the Arduino extension board. Tap it to go to the next page. Hold it to go back a page.

A single button could also be enough for the book list GUI. Perhaps double-tap to enter a book, tap to select next book, hold to select previous book.

It's very limiting to have a single button, but I like the idea of that simplicity for a v1.0.
