/*
 * Pranae Harirajan, phari012@ucr.edu
 * TurretLock - Demo 2
 *
 * The game in one paragraph:
 *   The LED matrix is the playfield. There are walls on the far left and far
 *   right. Somewhere in the middle sits a 2x2 "target". You move a single dot
 *   around with the joystick. When your dot is touching the target you are
 *   "locked on" (red LED solid, green LED blinks once). Press the joystick
 *   button while locked to score a point - the score goes up, a new target
 *   appears, and the number on the TFT screen updates. Two servos follow the
 *   joystick the whole time, like a little turret aiming around.
 *
 * Style note: this is bare-metal AVR (no Arduino.h). We write our own main(),
 * which quietly replaces the one the Arduino core would normally provide, so
 * setup()/loop() never run and we get to set up the chip's timers ourselves.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "phari012_periph.h"
#include "phari012_max7219.h"
#include "phari012_tft.h"

// which pin is which
#define GREEN_LED   PD4    // D4
#define RED_LED     PD5    // D5
#define BUTTON      PD2    // D2  (joystick click, reads 0 when pressed)
#define SERVO_X     PC2    // A2  (left/right turret)
#define SERVO_Y     PC3    // A3  (up/down turret)

// the playfield is 32 columns wide and 8 rows tall
#define COLS        32
#define ROWS        8
#define WALL        4                 // each wall is 4 columns thick
#define LEFT_EDGE   (WALL)            // first column you're allowed in  = 4
#define RIGHT_EDGE  (COLS - WALL - 1) // last column you're allowed in   = 27

// the target is a 2x2 block, so its top-left corner has to leave room for it
#define TARGET_MIN_X  LEFT_EDGE       // 4
#define TARGET_MAX_X  (RIGHT_EDGE - 1)// 26
#define TARGET_MIN_Y  0
#define TARGET_MAX_Y  (ROWS - 2)      // 6

// How the aim dot moves. The joystick is spring-loaded back to center, so we
// treat it as a SPEED, not a position: push it and the dot keeps gliding that
// way; let go and the dot stays put (no more snapping back to the middle).
#define PLAY_W        (RIGHT_EDGE - LEFT_EDGE)  // playable columns wide = 23
#define PLAY_H        (ROWS - 1)                // playable rows tall     = 7
#define FINE          64        // sub-steps per cell, so slow moves feel smooth
#define JOY_DEADZONE  90        // ignore small wiggle near center (out of ~512)
#define JOY_SPEED_DIV 28        // bigger = slower dot; smaller = faster dot

// Servo pulse lengths, measured in Timer1 ticks (each tick = 0.5 microseconds).
// A servo wants a pulse every 20ms; the WIDTH of that pulse is the angle.
// 1ms = 2000 ticks, so these are roughly 0.5ms .. 1.5ms .. 2.5ms.
#define SERVO_LEFT    1000
#define SERVO_CENTER  3000
#define SERVO_RIGHT   5000


// servos
//
// Instead of busy-waiting to make the servo pulses (which would freeze the
// game), Timer1 does it in the background. Every 20ms it raises both servo
// pins, and a moment later (after OCR1A / OCR1B ticks) it drops them again.
// That "moment" is the pulse width, which is the angle. The main loop just
// writes the angle it wants into OCR1A / OCR1B and forgets about it.
static void setupServos(void) {
    DDRC |= (1 << SERVO_X) | (1 << SERVO_Y);          // A2, A3 are outputs
    PORTC &= ~((1 << SERVO_X) | (1 << SERVO_Y));

    // Fast PWM with ICR1 as the top-of-count. We don't use the chip's auto
    // output pins (we flip PC2/PC3 by hand in the interrupts below instead).
    TCCR1A = (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);  // /8 clock
    ICR1   = 39999;                                       // count = one 20ms frame
    OCR1A  = SERVO_CENTER;
    OCR1B  = SERVO_CENTER;

    TIMSK1 = (1 << TOIE1) | (1 << OCIE1A) | (1 << OCIE1B); // turn on the 3 interrupts
}

ISR(TIMER1_OVF_vect)   { PORTC |=  (1 << SERVO_X) | (1 << SERVO_Y); } // frame start: pins up
ISR(TIMER1_COMPA_vect) { PORTC &= ~(1 << SERVO_X); }                  // X pulse done: pin down
ISR(TIMER1_COMPB_vect) { PORTC &= ~(1 << SERVO_Y); }                  // Y pulse done: pin down


// serial (just so we can watch the score in the monitor)
static void setupSerial(void) {
    UBRR0H = 0;
    UBRR0L = 103;                                  // 103 gives 9600 baud at 16MHz
    UCSR0B = (1 << TXEN0);                          // turn on the transmitter
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);         // 8 data bits, no parity, 1 stop
}
static void serialChar(char c) {
    while (!(UCSR0A & (1 << UDRE0))) { }            // wait until it's ready for a byte
    UDR0 = c;
}
static void serialText(const char *s) { while (*s) serialChar(*s++); }
static void serialNumber(uint16_t n) {
    char digits[6]; uint8_t i = 0;
    if (n == 0) { serialChar('0'); return; }
    while (n) { digits[i++] = '0' + (n % 10); n /= 10; }
    while (i--) serialChar(digits[i]);             // we built it backwards, so print backwards
}

// turn a number like 42 into the text "42"
static void numberToText(uint16_t n, char *out) {
    char digits[6]; uint8_t i = 0;
    if (n == 0) { out[0] = '0'; out[1] = '\0'; return; }
    while (n) { digits[i++] = '0' + (n % 10); n /= 10; }
    uint8_t j = 0;
    while (i--) out[j++] = digits[i];
    out[j] = '\0';
}

// Show the score. The word "SCORE" is painted once in main(); here we only
// repaint the number. We wipe its area to black first, otherwise going from
// "10" back to "9" would leave the old "0" sitting there.
#define SCORE_X     8
#define SCORE_Y     46
#define SCORE_SIZE  4
static void showScore(uint16_t score) {
    serialText("Score: ");
    serialNumber(score);
    serialText("\r\n");

    char text[6];
    numberToText(score, text);
    tft_fill_rect(SCORE_X, SCORE_Y, TFT_W - 2 * SCORE_X, 8 * SCORE_SIZE, TFT_BLACK);
    tft_draw_text(SCORE_X, SCORE_Y, text, TFT_CYAN, TFT_BLACK, SCORE_SIZE);
}


// random numbers
//
// A tiny home-made random generator. It just shuffles its own bits around.
// Not "real" randomness, but plenty good for dropping a target somewhere.
static uint16_t randomState = 0xACE1;
static uint16_t randomNext(void) {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    randomState ^= randomState << 8;
    return randomState;
}


// drawing on the matrix
//
// "grid" is our picture of the screen before we send it out: 8 rows, and each
// row is split across the 4 chained matrix modules. One bit = one LED.
static inline void lightPixel(uint8_t grid[8][4], uint8_t col, uint8_t row) {
    if (col < COLS && row < ROWS)
        grid[row][col >> 3] |= (uint8_t)(1 << (col & 7));   // col>>3 picks the module, col&7 the LED
}
static void clearGrid(uint8_t grid[8][4]) {
    for (uint8_t r = 0; r < 8; r++)
        for (uint8_t m = 0; m < 4; m++)
            grid[r][m] = 0x00;
}
static void drawWalls(uint8_t grid[8][4]) {
    for (uint8_t r = 0; r < 8; r++) {
        grid[r][0] |= 0x0F;   // light up columns 0..3   (left wall)
        grid[r][3] |= 0xF0;   // light up columns 28..31 (right wall)
    }
}


// game state
static uint8_t  aimX, aimY;          // where the dot is
static uint8_t  targetX, targetY;    // top-left corner of the 2x2 target
static uint16_t score;
static uint8_t  lockedOn;            // is the dot touching the target right now?

// drop a fresh target somewhere legal in the play area
static void newTarget(void) {
    targetX = TARGET_MIN_X + (randomNext() % (TARGET_MAX_X - TARGET_MIN_X + 1));
    targetY = TARGET_MIN_Y + (randomNext() % (TARGET_MAX_Y - TARGET_MIN_Y + 1));
}

// We're "locked" when the dot is on any of the 4 cells of the 2x2 target.
static uint8_t checkLock(void) {
    return (aimX == targetX || aimX == targetX + 1) &&
           (aimY == targetY || aimY == targetY + 1);
}


// main
int main(void) {
    // LEDs are outputs, button is an input with the chip's pull-up resistor on
    DDRD  |= (1 << GREEN_LED) | (1 << RED_LED);
    DDRD  &= ~(1 << BUTTON);
    PORTD |= (1 << BUTTON);                              // pull-up: button reads 1 until pressed
    PORTD &= ~((1 << GREEN_LED) | (1 << RED_LED));       // both LEDs start off

    // wake up all the hardware
    setupSerial();
    ADC_init();
    MAX_pins_init();
    MAX_init();
    setupServos();
    sei();                                              // let the servo interrupts run

    // screen: flash blue for a second (proves it's alive), then set up the score area
    tft_init();
    tft_fill_screen(TFT_BLUE);
    _delay_ms(250);
    tft_fill_screen(TFT_BLACK);
    tft_draw_text(8, 10, "SCORE", TFT_WHITE, TFT_BLACK, 2);

    // mix the joystick's noisy readings into the random seed so each game differs
    randomState ^= (uint16_t)(ADC_read(0) << 6) ^ (uint16_t)ADC_read(1);
    if (randomState == 0) randomState = 0xBEEF;
    newTarget();

    aimX = (LEFT_EDGE + RIGHT_EDGE) / 2;                 // start the dot in the middle
    aimY = ROWS / 2;
    score = 0;
    lockedOn = 0;

    serialText("\r\nTurretLock Demo 2 - aim with joystick, press to fire.\r\n");
    showScore(score);

    uint8_t grid[8][4];
    uint8_t buttonLast    = 1;   // 1 = released (remember last frame so we catch the press)
    uint8_t wasLockedOn   = 0;
    uint8_t greenFlash    = 0;   // counts down the brief green flash after a lock
    uint8_t blinkOn       = 0;   // flips on/off to make things blink
    uint8_t loopCount     = 0;
    int16_t fineX = (PLAY_W * FINE) / 2;   // dot's fine position, starts centered
    int16_t fineY = (PLAY_H * FINE) / 2;

    while (1) {
        // read the joystick (0..1023 on each axis)
        uint16_t joyX = ADC_read(0);
        uint16_t joyY = ADC_read(1);

        // how hard, and which way, the stick is pushed from its center (~512)
        int16_t pushX = (int16_t)joyX - 512;
        int16_t pushY = (int16_t)joyY - 512;

        // ignore small wiggle so a resting or slipping stick doesn't drift
        if (pushX > -JOY_DEADZONE && pushX < JOY_DEADZONE) pushX = 0;
        if (pushY > -JOY_DEADZONE && pushY < JOY_DEADZONE) pushY = 0;

        // glide the dot by that push (a speed). Let go -> push is 0 -> dot holds.
        fineX += pushX / JOY_SPEED_DIV;
        fineY -= pushY / JOY_SPEED_DIV;   // minus so pushing up moves the dot up

        // keep the dot inside the play area
        if (fineX < 0) fineX = 0;
        if (fineX > PLAY_W * FINE) fineX = PLAY_W * FINE;
        if (fineY < 0) fineY = 0;
        if (fineY > PLAY_H * FINE) fineY = PLAY_H * FINE;

        // the matrix cell the dot is in right now
        aimX = LEFT_EDGE + (uint8_t)(fineX / FINE);
        aimY = (uint8_t)(fineY / FINE);

        // point the servos at wherever the dot is, so the turret holds too
        OCR1A = (uint16_t)map_value(LEFT_EDGE, RIGHT_EDGE, SERVO_LEFT,  SERVO_RIGHT, aimX);
        OCR1B = (uint16_t)map_value(0, ROWS - 1,          SERVO_RIGHT, SERVO_LEFT,  aimY);

        // are we on the target?
        lockedOn = checkLock();
        if (lockedOn && !wasLockedOn) greenFlash = 8;   // just locked on -> blink green for ~8 frames

        // fire: only counts on the frame the button goes from released to pressed
        uint8_t button = (PIND >> BUTTON) & 1;          // 1 released, 0 pressed
        if (buttonLast == 1 && button == 0) {
            if (lockedOn) {
                score++;
                showScore(score);
                newTarget();
                lockedOn = 0;                           // have to re-aim at the new target
            }
        }
        buttonLast = button;

        // red LED: solid when locked, slow blink while a target is waiting
        if (lockedOn || blinkOn) PORTD |= (1 << RED_LED);
        else                     PORTD &= ~(1 << RED_LED);
        // green LED: short flash right after locking on
        if (greenFlash) { PORTD |= (1 << GREEN_LED); greenFlash--; }
        else            { PORTD &= ~(1 << GREEN_LED); }

        // draw the playfield: walls, then the target, then the dot on top
        clearGrid(grid);
        drawWalls(grid);
        if (!lockedOn || blinkOn) {                     // blink the target while locked on
            lightPixel(grid, targetX,     targetY);
            lightPixel(grid, targetX + 1, targetY);
            lightPixel(grid, targetX,     targetY + 1);
            lightPixel(grid, targetX + 1, targetY + 1);
        }
        lightPixel(grid, aimX, aimY);
        MAX_render(grid);

        // bookkeeping for next time around
        wasLockedOn = lockedOn;
        if ((++loopCount & 0x0F) == 0) blinkOn ^= 1;    // flip the blink every 16 frames
        _delay_ms(15);
    }

    return 0;
}
