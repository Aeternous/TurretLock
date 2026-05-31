#ifndef MAX7219_H
#define MAX7219_H

#include <avr/io.h>
#include "phari012_periph.h"

#ifdef __cplusplus
extern "C" {
#endif

// which Uno pins the matrix uses:
// D10 = PB2 (CS), D11 = PB3 (data), D13 = PB5 (clock)
#define MAX_CS_BIT   PB2
#define MAX_DIN_BIT  PB3
#define MAX_CLK_BIT  PB5

void MAX_pins_init(void);                 // set up the three pins
void SPI_SEND(uint8_t value);             // shift out one byte by hand
void MAX_send_all(uint8_t reg, uint8_t data);  // same byte to all four chips
void MAX_init(void);                      // wake-up routine
void MAX_all_on(void);                    // light everything (wiring test)
void MAX_cursor(uint8_t col, uint8_t row);     // old smoke-test helper

// draw a whole picture: 8 rows, each spread over the 4 matrix modules.
// grid[row][module] holds 8 dots (one bit each).
void MAX_render(const uint8_t grid[8][4]);

#ifdef __cplusplus
}
#endif

#endif /* MAX7219_H */