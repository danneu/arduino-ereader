#include "spi.h"

#include <avr/io.h>

#define SS 2    // PB2
#define SCK 5   // PB5
#define MISO 4  // PB4
#define MOSI 3  // PB3

void spi_begin() {
    PORTB |= (1 << SS);  // SS HIGH to deselect any connected chip
    DDRB |= (1 << SS);   // SS OUTPUT

    SPCR |= (1 << MSTR);  // Set as Master
    SPCR |= (1 << SPE);   // Enable SPI

    // Set to OUTPUT after SPI enabled
    DDRB |= (1 << MOSI) | (1 << SCK);
}

// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
uint8_t spi_xfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))  // Wait until transferred
        ;
    return SPDR;
}

uint8_t spi_recv() { return spi_xfer(0xff); }