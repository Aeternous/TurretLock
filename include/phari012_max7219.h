/*        Your Name & E-mail:
Pranae Harirajan, phari012@ucr.edu
 *        Discussion Section:
RB

 *        Assignment: Custom Laboratory Project - Demo Video #2

 *        Exercise Description: Header for the MAX7219 4-in-1 LED matrix driver

 *        I acknowledge all content contained herein, excluding template or example code, is my own original work.
yes
 */

#ifndef MAX7219_H
#define MAX7219_H

#include <avr/io.h>
#include "phari012_periph.h"

#ifdef __cplusplus
extern "C" {
#endif

// Which Uno pins the matrix uses:
// D10 = PB2 (CS), D11 = PB3 (data), D13 = PB5 (clock)
#define MAX_CS_BIT   PB2
#define MAX_DIN_BIT  PB3
#define MAX_CLK_BIT  PB5

void matrixSetupPins();                              // set up the three pins
void sendByte(unsigned char value);                  // shift out one byte by hand
void matrixSendAll(unsigned char reg, unsigned char data); // same byte to all four chips
void matrixInit();                                   // full wake-up (config + clear)
void matrixRefreshConfig();                          // re-send config (heals glitches)
void matrixAllOn();                                  // light everything (wiring test)

// Draw a whole picture: 8 rows, each spread over the 4 matrix modules.
// grid[row][module] holds 8 dots (one bit each).
void matrixDraw(const unsigned char grid[8][4]);

#ifdef __cplusplus
}
#endif

#endif /* MAX7219_H */
