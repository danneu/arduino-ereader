const fs = require('fs')
const { join } = require('path')
// http://unifoundry.com/unifont/index.html

const text = fs.readFileSync(require('path').join(__dirname, './unifont-11.0.02.hex'), { encoding: 'utf8' })

const CONFIG = {
    w: 8, // px
    h: 16,
}

const ranges = [
    // C0 Controls and Basic Latin
    // start at space (32) instead of 0x0000
    [32, 0x007f],
    // C1 Controls and Latin-1 Supplement
    // [0x0080, 0x00ff],
    [160, 0x00ff],
    // Latin Extended - A
    [0x0100, 0x017f],
    // Latin Extended - B
    [0x0180, 0x024f],
    // General Punctuation
    // [0x2000, 0x206f],
    [0x2000, 8287],
]

const emptyIds = new Set()

// a mapping of rangeidx to [glyphs]
const glyphs = {}
for (let i = 0; i < ranges.length; i++) {
    glyphs[i] = []
}

let i = 0
let empties = 0
for (const line of text.split('\n')) {
    let [id, data] = line.split(':')
    if (!data) continue
    id = Number.parseInt(id, 16)
    let bytes = data.match(/.{1,2}/g).map((s) => Number.parseInt(s, 16))

    let rangeidx = 0
    for (const [min, max] of ranges) {
        if (id >= min && id <= max) {
            if (bytes.length > CONFIG.h) {
                empties++
                bytes = []
                emptyIds.add(id)
            }
            glyphs[rangeidx].push({
                id,
                bytes,
            })
            break
        }
        rangeidx++
    }

    //if (data.length>32) continue
    // if (data.length>32) {
    //   data = empty
    //   empties++
    // }
    //console.log(i,id, data)

    // console.log(
    //     data
    //         .match(/.{1,2}/g)
    //         .map((x) => '0x' + x)
    //         .join(',') + ','
    // )

    //if (i++ > 10)  break
}
console.log('empties', empties)

const fd = fs.openSync(join(__dirname, '../src/glyphs.c'), 'w')
fs.ftruncateSync(fd)
const append = (str) => fs.writeFileSync(fd, str, 'utf8', 'as')

append('#include "glyphs.h"\n')
append('#include <stdint.h>\n')
append('#include <avr/pgmspace.h>\n')
append('\n')

// const PROGMEM char *const ptr = "NUMBER1";
// is a shortcut for
// const char spaceinmemory[] = "NUMBER1";
// const char* ptr PROGMEM = spaceinmemory;
for (const [rangeidx, gs] of Object.entries(glyphs)) {
    i = 0
    for (const glyph of gs) {
        const hex = glyph.bytes.map((s) => '0x' + s.toString(16).padStart(2, '0')).join(',')
        // const uint8_t glyph0[16] PROGMEM = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
        append(`const uint8_t glyph_${rangeidx}_${glyph.id}[${glyph.bytes.length}] PROGMEM = {${hex}};\n`)
        i++
    }

    const [min, max] = ranges[rangeidx]

    append(`\n\nconst uint8_t *const glyphs_${rangeidx}[${gs.length}] PROGMEM = {\n\t`)
    for (let i = 0; i < gs.length; i++) {
        append(`glyph_${rangeidx}_${gs[i].id}`)
        append(i < gs.length - 1 ? ', ' : '\n')
        if (i > 0 && i % 8 == 0) append('\n\t')
    }
    append('};\n')
    // const uint8_t *const unicode_glyphs[560] PROGMEM = {
    //     glyph0, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6, glyph7,
    //     glyph8, glyph9, glyph10, glyph11, glyph12, glyph13, glyph14,
}

// Now the lookup fn
/*
	uint8_t y;
	for (y = 0; y < CHAR_HEIGHT; y++) {
        uint8_t glyph[16];
        memcpy_P(glyph, pgm_read_word(&(unicode_glyphs[c])), 16  );
        textrow[WIDTH/8*y + idx] = ~glyph[y]; // invert cuz char data uses black=clear
    }

// returns 0 if there was match
int get_glyph(uint32_t codepoint, uint8_t idx, uint8_t * buf) {
    if (codepoint >= min && codepoint <= max) {
        uint8_t glyph[${CONFIG.h}];
        memcpy_P(glyph, pgm_read_word(&(glyphs_${rangeidx}[codepoint-${min}])), ${CONFIG.h}  );
        buf[0] = glyph[idx];
        return 1;
    } else if () {

    } else {
        return 0;
    }
}
*/

// append(`\nint get_glyph(uint32_t codepoint, uint8_t idx, uint8_t * buf) {\n`)
// for (const [rangeidx, gs] of Object.entries(glyphs)) {
//     const [min, max] = ranges[rangeidx]
//     append(rangeidx === '0' ? '\tif ' : '\t} else if ')
//     append(`(codepoint >= ${min} && codepoint <= ${max}) {\n`)
//     append(`\t\tmemcpy_P(buf, (const void *)pgm_read_word(&(glyphs_${rangeidx}[codepoint-${min}])) + idx, 1  );\n`)
//     append(`\t\treturn 1;\n`)
// }
// append(`\t} else {\n\t\treturn 0;\n\t}\n`)
// append(`}\n`)

////////////////////////////////////////////////////////////

// append(`\nint get_glyph2(uint32_t codepoint, uint8_t idx, uint8_t * buf) {\n`)
// append(`\tif (idx > ${CONFIG.h - 1}) return 0;\n`)
// for (const [rangeidx, gs] of Object.entries(glyphs)) {
//     const [min, max] = ranges[rangeidx]
//     append(rangeidx === '0' ? '\tif ' : '\t} else if ')
//     append(`(codepoint >= ${min} && codepoint <= ${max}) {\n`)
//     // auto glyph_idx = 2;
//     // auto row_idx = 2;
//     // auto byte = pgm_read_byte(pgm_read_ptr(things + glyph_idx) + row_idx);
//     append(`\t\t*buf = pgm_read_byte(pgm_read_ptr(glyphs_${rangeidx}+codepoint-${min})+idx);\n`)
//     append(`\t\treturn 1;\n`)
// }
// append(`\t} else {\n\t\treturn 0;\n\t}\n`)
// append(`}\n`)

////////////////////////////////////////////////////////////

append(`\nuint8_t get_glyph(uint32_t pt, uint8_t buf[16]) {\n`)
for (const id of emptyIds) {
    append(`if (pt == ${id}) return 1;\n`)
}
for (const [rangeidx, gs] of Object.entries(glyphs)) {
    const [min, max] = ranges[rangeidx]
    append(rangeidx === '0' ? '\tif ' : '\t} else if ')
    append(`(${min === 0 ? '' : `pt >= ${min} && `}pt <= ${max}) {\n`)
    append(`\t\tmemcpy_P(buf, (void *)pgm_read_word(&(glyphs_${rangeidx}[pt-${min}])), 16);\n`)
    append(`\t\treturn 0;\n`)
}
append(`\t} else {\n\t\treturn 1;\n\t}\n`)
append(`}\n`)

/*
 00
 00
 00
 00
 10 '00010000'
 10  00010000
 00  00000000
 10  00010000
 10  00010000
 20
 40
 42
 42
 3C
 00
 00

*/
