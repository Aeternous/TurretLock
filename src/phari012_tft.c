/*
 * Pranae Harirajan, phari012@ucr.edu
 * Tiny ST7735 TFT driver, written from scratch (no libraries).
 *
 * How it talks to the screen: same idea as the LED matrix - we wiggle a data
 * pin and a clock pin by hand to shove bytes out (we reuse SPI_SEND from the
 * matrix code, since they share those two pins). The screen also needs three
 * of its own control lines:
 *   CS  - "I'm talking to YOU" (low while we send to the TFT)
 *   DC  - tells the screen whether a byte is a command or pixel data
 *   RST - hardware reset
 * All of this runs from main(), so it never collides with the servo interrupts.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "phari012_tft.h"
#include "phari012_max7219.h"   // SPI_SEND(), and the shared data/clock pins

// the screen's control pins
#define TFT_CS   PB1   // D9
#define TFT_DC   PB0   // D8
#define TFT_RST  PD7   // D7

// If the picture is nudged a couple pixels off, tweak these. Red-tab 1.8"
// boards use 0,0; green-tab boards often need 2 and 1.
#define COL_OFFSET 0
#define ROW_OFFSET 0

// little shortcuts so the code reads like plain instructions
#define selectTFT()    (PORTB &= ~(1 << TFT_CS))   // start talking to the screen
#define releaseTFT()   (PORTB |=  (1 << TFT_CS))   // done talking
#define modeCommand()  (PORTB &= ~(1 << TFT_DC))   // next byte is a command
#define modeData()     (PORTB |=  (1 << TFT_DC))   // next byte is data
#define resetLow()     (PORTD &= ~(1 << TFT_RST))
#define resetHigh()    (PORTD |=  (1 << TFT_RST))

static inline void sendCommand(uint8_t c) { modeCommand(); SPI_SEND(c); }
static inline void sendData(uint8_t d)    { modeData();    SPI_SEND(d); }


// the font
//
// Each letter is 5 columns of 7 dots. A byte holds one column; bit 0 is the
// top dot, bit 6 the bottom. We only included the characters we actually use.
static const uint8_t FONT_DIGITS[] PROGMEM = {
    0x3E,0x51,0x49,0x45,0x3E,  // 0
    0x00,0x42,0x7F,0x40,0x00,  // 1
    0x42,0x61,0x51,0x49,0x46,  // 2
    0x21,0x41,0x45,0x4B,0x31,  // 3
    0x18,0x14,0x12,0x7F,0x10,  // 4
    0x27,0x45,0x45,0x45,0x39,  // 5
    0x3C,0x4A,0x49,0x49,0x30,  // 6
    0x01,0x71,0x09,0x05,0x03,  // 7
    0x36,0x49,0x49,0x49,0x36,  // 8
    0x06,0x49,0x49,0x29,0x1E,  // 9
};
static const uint8_t FONT_LETTERS[] PROGMEM = {
    0x7E,0x11,0x11,0x11,0x7E,  // A
    0x7F,0x49,0x49,0x49,0x36,  // B
    0x3E,0x41,0x41,0x41,0x22,  // C
    0x7F,0x41,0x41,0x22,0x1C,  // D
    0x7F,0x49,0x49,0x49,0x41,  // E
    0x7F,0x09,0x09,0x09,0x01,  // F
    0x3E,0x41,0x49,0x49,0x7A,  // G
    0x7F,0x08,0x08,0x08,0x7F,  // H
    0x00,0x41,0x7F,0x41,0x00,  // I
    0x20,0x40,0x41,0x3F,0x01,  // J
    0x7F,0x08,0x14,0x22,0x41,  // K
    0x7F,0x40,0x40,0x40,0x40,  // L
    0x7F,0x02,0x0C,0x02,0x7F,  // M
    0x7F,0x04,0x08,0x10,0x7F,  // N
    0x3E,0x41,0x41,0x41,0x3E,  // O
    0x7F,0x09,0x09,0x09,0x06,  // P
    0x3E,0x41,0x51,0x21,0x5E,  // Q
    0x7F,0x09,0x19,0x29,0x46,  // R
    0x46,0x49,0x49,0x49,0x31,  // S
    0x01,0x01,0x7F,0x01,0x01,  // T
    0x3F,0x40,0x40,0x40,0x3F,  // U
    0x1F,0x20,0x40,0x20,0x1F,  // V
    0x3F,0x40,0x38,0x40,0x3F,  // W
    0x63,0x14,0x08,0x14,0x63,  // X
    0x07,0x08,0x70,0x08,0x07,  // Y
    0x61,0x51,0x49,0x45,0x43,  // Z
};
static const uint8_t FONT_COLON[5] PROGMEM = { 0x00, 0x00, 0x14, 0x00, 0x00 };

// copy the 5 columns of one character into 'out'
static void getLetterShape(char c, uint8_t out[5]) {
    const uint8_t *shape = 0;
    if (c >= '0' && c <= '9')      shape = &FONT_DIGITS[(c - '0') * 5];
    else if (c >= 'A' && c <= 'Z') shape = &FONT_LETTERS[(c - 'A') * 5];
    else if (c == ':')             shape = FONT_COLON;
    for (uint8_t i = 0; i < 5; i++)
        out[i] = shape ? pgm_read_byte(&shape[i]) : 0x00;   // unknown char = blank
}


// drawing
//
// Tell the screen which rectangle we're about to fill, then it expects that
// many pixels (2 bytes each) until we stop.
static void setDrawArea(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    sendCommand(0x2A);                         // set column range
    sendData(0); sendData(x0 + COL_OFFSET);
    sendData(0); sendData(x1 + COL_OFFSET);
    sendCommand(0x2B);                         // set row range
    sendData(0); sendData(y0 + ROW_OFFSET);
    sendData(0); sendData(y1 + ROW_OFFSET);
    sendCommand(0x2C);                         // "here come the pixels"
}

void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (x >= TFT_W || y >= TFT_H) return;
    if (x + w > TFT_W) w = TFT_W - x;          // don't draw off the edge
    if (y + h > TFT_H) h = TFT_H - y;

    uint8_t colorHigh = (uint8_t)(color >> 8);
    uint8_t colorLow  = (uint8_t)(color & 0xFF);

    selectTFT();
    setDrawArea((uint8_t)x, (uint8_t)y, (uint8_t)(x + w - 1), (uint8_t)(y + h - 1));
    modeData();
    for (uint32_t pixels = (uint32_t)w * (uint32_t)h; pixels; pixels--) {
        SPI_SEND(colorHigh);
        SPI_SEND(colorLow);
    }
    releaseTFT();
}

void tft_fill_screen(uint16_t color) {
    tft_fill_rect(0, 0, TFT_W, TFT_H, color);
}

// draw one character as a grid of little squares (size = how many real pixels
// wide each font-dot becomes, so size 4 makes nice big digits)
void tft_draw_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size) {
    if (size == 0) size = 1;
    uint8_t shape[5];
    getLetterShape(c, shape);
    for (uint8_t col = 0; col < 6; col++) {            // 5 columns + 1 blank gap
        uint8_t columnBits = (col < 5) ? shape[col] : 0x00;
        for (uint8_t row = 0; row < 8; row++) {        // 7 rows + 1 blank gap
            uint16_t color = (columnBits & (1 << row)) ? fg : bg;
            tft_fill_rect(x + col * size, y + row * size, size, size, color);
        }
    }
}

void tft_draw_text(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t size) {
    if (size == 0) size = 1;
    while (*s) {
        tft_draw_char(x, y, *s, fg, bg, size);
        x += 6 * size;                                 // step over to the next character slot
        s++;
    }
}


// startup
//
// This is the magic-number part: a fixed sequence the ST7735 chip wants to
// hear before it'll show anything (wake up, set colors to 16-bit, gamma, etc).
// It comes straight from the chip's datasheet - you set it once and forget it.
void tft_init(void) {
    DDRB |= (1 << TFT_CS) | (1 << TFT_DC) | (1 << MAX_DIN_BIT) | (1 << MAX_CLK_BIT);
    DDRD |= (1 << TFT_RST);
    releaseTFT();

    // give the screen a hard reset
    resetHigh(); _delay_ms(10);
    resetLow();  _delay_ms(10);
    resetHigh(); _delay_ms(120);

    selectTFT();
    sendCommand(0x01); _delay_ms(150);                            // software reset
    sendCommand(0x11); _delay_ms(120);                            // wake from sleep

    sendCommand(0xB1); sendData(0x01); sendData(0x2C); sendData(0x2D);   // refresh timing
    sendCommand(0xB2); sendData(0x01); sendData(0x2C); sendData(0x2D);
    sendCommand(0xB3); sendData(0x01); sendData(0x2C); sendData(0x2D);
                       sendData(0x01); sendData(0x2C); sendData(0x2D);
    sendCommand(0xB4); sendData(0x07);                            // inversion off

    sendCommand(0xC0); sendData(0xA2); sendData(0x02); sendData(0x84);   // power settings
    sendCommand(0xC1); sendData(0xC5);
    sendCommand(0xC2); sendData(0x0A); sendData(0x00);
    sendCommand(0xC3); sendData(0x8A); sendData(0x2A);
    sendCommand(0xC4); sendData(0x8A); sendData(0xEE);
    sendCommand(0xC5); sendData(0x0E);                            // contrast-ish (VCOM)

    sendCommand(0x20);                                           // colors not inverted
    sendCommand(0x36); sendData(0x08);                           // orientation (portrait, flipped 180)
    sendCommand(0x3A); sendData(0x05);                           // 16-bit color

    sendCommand(0xE0);                                          // gamma curve (brightness shape)
    sendData(0x0F); sendData(0x1A); sendData(0x0F); sendData(0x18);
    sendData(0x2F); sendData(0x28); sendData(0x20); sendData(0x22);
    sendData(0x1F); sendData(0x1B); sendData(0x23); sendData(0x37);
    sendData(0x00); sendData(0x07); sendData(0x02); sendData(0x10);
    sendCommand(0xE1);
    sendData(0x0F); sendData(0x1B); sendData(0x0F); sendData(0x17);
    sendData(0x33); sendData(0x2C); sendData(0x29); sendData(0x2E);
    sendData(0x30); sendData(0x30); sendData(0x39); sendData(0x3F);
    sendData(0x00); sendData(0x07); sendData(0x03); sendData(0x10);

    sendCommand(0x13); _delay_ms(10);                            // normal display mode
    sendCommand(0x29); _delay_ms(100);                           // turn the display on
    releaseTFT();
}
