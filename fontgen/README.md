# fontgen

`index.js` is a Node.js script that reads Unifont glyphs from `unifont.hex` and writes a
`glyphs.c` file with each glyph represented as a C byte array. It also generates the
`get_glyph()` C function that maps 32 byte Unicode codepoints into the corresponding `glyph[16]`.

Unicode glyphs are of course stored in and retrieved from program (flash) memory since they
take up so much space.

The script only selects from a few Unicode ranges that represent the most common western languages.
Many Unicode ranges aren't even used for written languages and can be discarded.

Another important note is that my e-reader code assumes monospaced glyphs. Even if I had room to store, say, various eastern language glyphs, many of those glyphs are twice the width and my code doesn't yet work with glyphs of different widths.

My code currently selects from specific Unicode point ranges and replaces glyphs that are too wide
with zeroed out data.

## TODO

-   Support all of displayable Unicode.
-   Support glyphs of different lengths.
-   Better optimization strategy.
