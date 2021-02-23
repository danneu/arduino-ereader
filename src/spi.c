#include "spi.h"

#include <avr/io.h>

#define SS 2    // PB2
#define SCK 5   // PB5
#define MISO 4  // PB4
#define MOSI 3  // PB3

#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif

#define SPI_CLOCK_DIV4 0x00
#define SPI_CLOCK_DIV16 0x01
#define SPI_CLOCK_DIV64 0x02
#define SPI_CLOCK_DIV128 0x03
#define SPI_CLOCK_DIV2 0x04
#define SPI_CLOCK_DIV8 0x05
#define SPI_CLOCK_DIV32 0x06

#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C

#define SPI_MODE_MASK 0x0C     // CPOL = bit 3, CPHA = bit 2 on SPCR
#define SPI_CLOCK_MASK 0x03    // SPR1 = bit 1, SPR0 = bit 0 on SPCR
#define SPI_2XCLOCK_MASK 0x01  // SPI2X = bit 0 on SPSR

//     #define CS_LOW() PORTB &= ~(1 << DDB2)
// #define CS_HIGH() PORTB |= (1 << DDB2)
void spi_begin() {
    PORTB |= (1 << SS);  // SS set HIGH to deselect any connected chip
    DDRB |= (1 << SS);   // SS HIGH

    SPCR |= (1 << MSTR);  // Set as Master
    SPCR |= (1 << SPE);   // Enable SPI

    // Set to OUTPUT after SPI enabled
    DDRB |= (1 << MOSI) | (1 << SCK);

    // SPCR |= (1 << SPR0) | (1 << SPR1);  // divided clock by 128
    // SPCR |= (1 << SPR0) | (1 << SPR1);  // divided clock by 128
    // uint32_t clockSetting = F_CPU / 2;
    // uint32_t clock = 4000000;
    // uint8_t clockDiv = 0;
    // while (clockDiv < 6 && clock < clockSetting) {
    //     clockSetting /= 2;
    //     clockDiv++;
    // }
    // // Compensate for the duplicate fosc/64
    // if (clockDiv == 6) clockDiv = 7;

    // // Invert the SPI2X bit
    // clockDiv ^= 0x1;

    // uint8_t bitOrder = MSBFIRST;
    // uint8_t dataMode = SPI_MODE0;

    // // Pack into the SPISettings class
    // SPCR = (1 << SPE) | (1 << MSTR) |
    //        ((bitOrder == LSBFIRST) ? (1 << DORD) : 0) |
    //        (dataMode & SPI_MODE_MASK) | ((clockDiv >> 1) & SPI_CLOCK_MASK);
    // SPSR = clockDiv & SPI_2XCLOCK_MASK;
}

// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
// inline static uint8_t spi_xfer(uint8_t data) {
//     SPDR = data;
//     /*
//      * The following NOP introduces a small delay that can prevent the wait
//      * loop form iterating when running at the maximum speed. This gives
//      * about 10% more speed, even if it seems counter-intuitive. At lower
//      * speeds it is unnoticed.
//      */
//     asm volatile("nop");
//     while (!(SPSR & (1 << SPIF)))  // Wait until transferred
//         ;
//     return SPDR;
// }

uint8_t spi_recv() { return spi_xfer(0xff); }