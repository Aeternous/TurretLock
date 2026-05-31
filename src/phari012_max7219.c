/*
 * Pranae Harirajan, phari012@ucr.edu
 * Driver for the chain of four 8x8 LED matrices (MAX7219 chips).
 *
 * We talk to the chips the simplest possible way: by hand-wiggling three pins
 * - CS (chip select), DIN (data), CLK (clock). Send a register number, then a
 * value. The four chips are daisy-chained, so a single CS-low...CS-high "burst"
 * feeds all four of them, one after another.
 */

#include "phari012_max7219.h"

// set up the three pins we use, and put them in their idle state
void MAX_pins_init(void) {
    DDRB |= (1 << MAX_CS_BIT) | (1 << MAX_DIN_BIT) | (1 << MAX_CLK_BIT);
    PORTB = SetBit(PORTB, MAX_CS_BIT, 1);   // CS idles high (not selected)
    PORTB = SetBit(PORTB, MAX_CLK_BIT, 0);
    PORTB = SetBit(PORTB, MAX_DIN_BIT, 0);
}

// shift out one byte, most-significant bit first
void SPI_SEND(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (value >> i) & 0x01;
        PORTB = SetBit(PORTB, MAX_DIN_BIT, bit);    // put the bit on the data line
        PORTB = SetBit(PORTB, MAX_CLK_BIT, 0);      // then tick the clock low...
        PORTB = SetBit(PORTB, MAX_CLK_BIT, 1);      // ...and high, which "clocks it in"
    }
}

// send the same register+value to all four chips at once
void MAX_send_all(uint8_t reg, uint8_t data) {
    PORTB = SetBit(PORTB, MAX_CS_BIT, 0);   // start the burst
    for (uint8_t chip = 0; chip < 4; chip++) {
        SPI_SEND(reg);
        SPI_SEND(data);
    }
    PORTB = SetBit(PORTB, MAX_CS_BIT, 1);   // end the burst (chips act on it now)
}

// the fixed wake-up routine the MAX7219 expects (from the chip's datasheet)
void MAX_init(void) {
    MAX_send_all(0x0C, 0x01);   // come out of shutdown -> run normally
    MAX_send_all(0x09, 0x00);   // no built-in digit decoding (we drive raw dots)
    MAX_send_all(0x0A, 0x08);   // brightness: middle
    MAX_send_all(0x0B, 0x07);   // use all 8 rows
    MAX_send_all(0x0F, 0x00);   // test mode off

    for (uint8_t row = 1; row <= 8; row++)  // start with a blank screen
        MAX_send_all(row, 0x00);
}

// light every LED (handy for a quick wiring check)
void MAX_all_on(void) {
    for (uint8_t row = 1; row <= 8; row++)
        MAX_send_all(row, 0xFF);
}

// turn everything on, then knock out a single LED at (col, row) - leftover from
// the early smoke test; the game uses MAX_render() instead.
void MAX_cursor(uint8_t col, uint8_t row) {
    MAX_all_on();

    uint8_t chip       = col / 8;   // which of the four modules
    uint8_t colOnChip  = col % 8;   // which column inside it
    uint8_t rowReg     = row + 1;   // the matrix numbers its rows 1..8

    PORTB = SetBit(PORTB, MAX_CS_BIT, 0);
    for (uint8_t c = 0; c < 4; c++) {
        uint8_t data = 0xFF;
        if (c == chip) data &= ~(1 << colOnChip);   // clear the one dot
        SPI_SEND(rowReg);
        SPI_SEND(data);
    }
    PORTB = SetBit(PORTB, MAX_CS_BIT, 1);
}

// draw a whole picture at once: 8 rows, each split across the 4 modules.
// grid[row][module] holds 8 dots (one bit each).
void MAX_render(const uint8_t grid[8][4]) {
    for (uint8_t row = 0; row < 8; row++) {
        PORTB = SetBit(PORTB, MAX_CS_BIT, 0);
        for (uint8_t module = 0; module < 4; module++) {
            SPI_SEND(row + 1);              // row number (1..8)
            SPI_SEND(grid[row][module]);    // its 8 dots for this module
        }
        PORTB = SetBit(PORTB, MAX_CS_BIT, 1);
    }
}
