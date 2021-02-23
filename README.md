# danbook

My 4.2" e-ink reader built with C on the Arduino.

I impulse purchased an Arduino hoping to learn some C and hardware, and I figured an e-ink display might be fun to hack on. Sure enough, it was.
This is my progress so far.

## Features

-   Uses less than 2K SRAM, as that's all the Arduino Uno/Nano has.
-   Loads ebooks from SD card (just .txt files for now)
-   UTF-8 decoding.
-   Unicode support for common languages.

## Goals

-   Stay fun.
-   Stay under 2K SRAM. Maybe 4K eventually.
-   Battery powered.
-   Epub support.

## Kit so far

-   $5 Arduino Nano or $15 Arduino Uno
-   $35 — [4.2in e-ink display](https://www.amazon.com/4-2inch-Module-Communicating-Resolution-Controller/dp/B074NR1SW2)
-   $7.50 — [https://www.adafruit.com/product/254](MicroSD card breakout board)

## Technical

Loading Arduino's SD card library immediately eats over 700 bytes of global variable memory which is way too aggressive.

I use the [Petit FAT](http://elm-chan.org/fsw/ff/00index_p.html) library which doesn't

## Thoughts

## Word wrap

