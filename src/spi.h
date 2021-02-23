
#pragma once

#ifndef spi_h
#define spi_h

#include <avr/io.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void spi_begin();
uint8_t spi_recv();
// uint8_t spi_xfer(uint8_t byte);

// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
inline static uint8_t spi_xfer(uint8_t data) {
    SPDR = data;
    /*
     * The following NOP introduces a small delay that can prevent the wait
     * loop form iterating when running at the maximum speed. This gives
     * about 10% more speed, even if it seems counter-intuitive. At lower
     * speeds it is unnoticed.
     */
    asm volatile("nop");
    while (!(SPSR & (1 << SPIF)))  // Wait until transferred
        ;
    return SPDR;
}

#ifdef __cplusplus
}
#endif

#endif  // guard