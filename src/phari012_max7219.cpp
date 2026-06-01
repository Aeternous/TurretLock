// Driver for the chain of four 8x8 LED matrices (MAX7219 chips).
// We talk to the chips by hand-wiggling three pins - CS (chip select),
// DIN (data), CLK (clock). Send a register number, then a value. The four
// chips are daisy-chained, so one CS-low...CS-high "burst" feeds all four.

#include "phari012_max7219.h"

// Set up the three pins we use, and put them in their idle state
void matrixSetupPins() {
  DDRB |= (1 << MAX_CS_BIT) | (1 << MAX_DIN_BIT) | (1 << MAX_CLK_BIT);
  PORTB = SetBit(PORTB, MAX_CS_BIT, 1);   // CS idles high (not selected)
  PORTB = SetBit(PORTB, MAX_CLK_BIT, 0);
  PORTB = SetBit(PORTB, MAX_DIN_BIT, 0);
}

// Shift out one byte, most-significant bit first
void sendByte(unsigned char value) {
  for (int i = 7; i >= 0; i--) {
    unsigned char bit = (value >> i) & 0x01;
    PORTB = SetBit(PORTB, MAX_DIN_BIT, bit);  // put the bit on the data line
    PORTB = SetBit(PORTB, MAX_CLK_BIT, 0);    // tick the clock low...
    PORTB = SetBit(PORTB, MAX_CLK_BIT, 1);    // ...and high, which clocks it in
  }
}

// Send the same register+value to all four chips at once
void matrixSendAll(unsigned char reg, unsigned char data) {
  PORTB = SetBit(PORTB, MAX_CS_BIT, 0);   // start the burst
  for (unsigned char chip = 0; chip < 4; chip++) {
    sendByte(reg);
    sendByte(data);
  }
  PORTB = SetBit(PORTB, MAX_CS_BIT, 1);   // end the burst (chips act on it now)
}

// (re)send the five setup registers. Doing this every so often "heals" a chip
// whose config got corrupted by noise (e.g. it flipped into the test mode that
// lights every LED).
void matrixRefreshConfig() {
  matrixSendAll(0x0C, 0x01);   // out of shutdown -> run normally
  matrixSendAll(0x09, 0x00);   // no built-in digit decoding (we drive raw dots)
  matrixSendAll(0x0A, 0x08);   // brightness: middle
  matrixSendAll(0x0B, 0x07);   // use all 8 rows
  matrixSendAll(0x0F, 0x00);   // test mode off
}

// Full wake-up: set the config, then start with a blank screen
void matrixInit() {
  matrixRefreshConfig();
  for (unsigned char row = 1; row <= 8; row++) {
    matrixSendAll(row, 0x00);
  }
}

// Light every LED (handy for a quick wiring check)
void matrixAllOn() {
  for (unsigned char row = 1; row <= 8; row++) {
    matrixSendAll(row, 0xFF);
  }
}

// Draw a whole picture at once: 8 rows, each split across the 4 modules.
// grid[row][module] holds 8 dots (one bit each).
void matrixDraw(const unsigned char grid[8][4]) {
  for (unsigned char row = 0; row < 8; row++) {
    PORTB = SetBit(PORTB, MAX_CS_BIT, 0);
    for (unsigned char module = 0; module < 4; module++) {
      sendByte(row + 1);             // row number (1..8)
      sendByte(grid[row][module]);   // its 8 dots for this module
    }
    PORTB = SetBit(PORTB, MAX_CS_BIT, 1);
  }
}
