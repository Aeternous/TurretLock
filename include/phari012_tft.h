#ifndef PHARI012_TFT_H
#define PHARI012_TFT_H

#include <avr/io.h>

#ifdef __cplusplus
extern "C" {
#endif

// ST7735 1.8" 128x160, portrait orientation.
// Control pins (AVR <- Uno):
//   TFT CS  -> D9  (PB1)
//   TFT DC  -> D8  (PB0)
//   TFT RST -> D7  (PD7)
//   TFT SDA -> D11 (PB3)  shared MOSI with MAX7219 DIN
//   TFT SCK -> D13 (PB5)  shared clock with MAX7219 CLK
// The shared MOSI/SCK is safe because each chip has its own CS line and we
// only ever drive one CS low at a time.

#define TFT_W 128
#define TFT_H 160

// 16-bit RGB565 colors
#define TFT_BLACK  0x0000
#define TFT_WHITE  0xFFFF
#define TFT_RED    0xF800
#define TFT_GREEN  0x07E0
#define TFT_BLUE   0x001F
#define TFT_YELLOW 0xFFE0
#define TFT_CYAN   0x07FF

void tft_init();
void tft_fill_screen(unsigned int color);
void tft_fill_rect(int x, int y, int w, int h, unsigned int color);
// 5x7 font, scaled by 'size'. Supports 0-9, A-Z, ':' and space (others blank).
void tft_draw_char(int x, int y, char c, unsigned int fg, unsigned int bg, unsigned char size);
void tft_draw_text(int x, int y, const char *s, unsigned int fg, unsigned int bg, unsigned char size);

#ifdef __cplusplus
}
#endif

#endif /* PHARI012_TFT_H */
