
#pragma once

#ifndef spi_h
#define spi_h

#include <avr/io.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void spi_begin();
uint8_t spi_xfer(uint8_t data);
uint8_t spi_recv();

#ifdef __cplusplus
}
#endif

#endif  // guard