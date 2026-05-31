/*
 * Pranae Harirajan, phari012@ucr.edu
 * TurretLock - Demo 2
 *
 * Demo Video #1: https://www.youtube.com/shorts/XU2ui8-Vj4o
 * Demo Video #2: <PASTE YOUR DEMO 2 YOUTUBE LINK HERE>
 *
 * The LED matrix is the playfield. Walls sit on the far left and far right; a
 * 2x2 target appears between them. You glide an aim dot around with the
 * joystick (push to move, let go to stay put). Land on the target and you lock
 * on - the red LED stops, the green LED stays lit. Press the joystick button
 * while locked to score: the number on the TFT goes up and a new target spawns.
 * Two servos act as a little turret that points wherever the dot is.
 *
 * Bare-metal AVR - no Arduino.h. We supply our own main(), which replaces the
 * one the Arduino core would give us, so we control the timers ourselves.
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

// pins
#define GREEN_LED  PD4    // D4
#define RED_LED    PD5    // D5
#define BUTTON     PD2    // D2  (joystick click: 0 when pressed)
#define SERVO_X    PC2    // A2
#define SERVO_Y    PC3    // A3

// playfield: 32 columns wide, 8 rows tall, with a wall on each side
#define COLS        32
#define ROWS        8
#define WALL_W      4                      // wall is 4 columns thick (matches proposal)
#define LEFT_EDGE   WALL_W                 // first column you can be in   = 4
#define RIGHT_EDGE  (COLS - 1 - WALL_W)    // last column you can be in    = 27

// Matrix wiring fixups. The four panels are chained together, and how they're
// wired decides the orientation. Three independent switches sort it out:
//   REVERSE_PANELS - which physical end is panel 1 (left vs right)
//   REVERSE_BITS   - which way columns count inside each single panel
//   FLIP_Y         - top vs bottom
#define REVERSE_PANELS  1
#define REVERSE_BITS    0
#define FLIP_Y          0

// joystick feel (it's spring-loaded, so we read it as a speed, not a position)
#define FINE          64    // sub-steps per cell, makes slow moves smooth
#define JOY_DEADZONE  90    // ignore small wiggle near center (out of ~512)
#define JOY_SPEED     28    // bigger = slower dot

// servo pulse widths in Timer1 ticks (0.5us each): ~0.5ms / 1.5ms / 2.5ms
#define SERVO_LEFT    1000
#define SERVO_MID     3000
#define SERVO_RIGHT   5000


// servos: Timer1 makes the pulses in the background so the game never stalls.
// Every 20ms it raises both pins; OCR1A / OCR1B decide when each drops again,
// and that pulse width is the angle. main() just writes the angle it wants.
static void setupServos(void) {
    DDRC  |= (1 << SERVO_X) | (1 << SERVO_Y);
    PORTC &= ~((1 << SERVO_X) | (1 << SERVO_Y));
    TCCR1A = (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);   // fast PWM, /8 clock
    ICR1   = 39999;                                        // 20ms frame
    OCR1A  = SERVO_MID;
    OCR1B  = SERVO_MID;
    TIMSK1 = (1 << TOIE1) | (1 << OCIE1A) | (1 << OCIE1B);
}
ISR(TIMER1_OVF_vect)   { PORTC |=  (1 << SERVO_X) | (1 << SERVO_Y); }  // frame start
ISR(TIMER1_COMPA_vect) { PORTC &= ~(1 << SERVO_X); }                   // X pulse end
ISR(TIMER1_COMPB_vect) { PORTC &= ~(1 << SERVO_Y); }                   // Y pulse end


// serial (so we can watch the score in the monitor)
static void setupSerial(void) {
    UBRR0L = 103;                                  // 9600 baud at 16MHz
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);        // 8N1
}
static void serialText(const char *s) {
    while (*s) {
        while (!(UCSR0A & (1 << UDRE0))) { }
        UDR0 = *s++;
    }
}

// turn a number like 42 into text "42"
static void numberToText(uint16_t n, char *out) {
    char rev[6]; uint8_t i = 0;
    if (n == 0) { out[0] = '0'; out[1] = '\0'; return; }
    while (n) { rev[i++] = '0' + (n % 10); n /= 10; }
    uint8_t j = 0;
    while (i--) out[j++] = rev[i];
    out[j] = '\0';
}

// Show the score. "SCORE" is painted once in main(); here we only repaint the
// number (wiping its box first so 10 -> 9 doesn't leave the old digit behind).
static void showScore(uint16_t score) {
    char text[6];
    numberToText(score, text);
    serialText("Score: "); serialText(text); serialText("\r\n");
    tft_fill_rect(8, 46, TFT_W - 16, 32, TFT_BLACK);
    tft_draw_text(8, 46, text, TFT_CYAN, TFT_BLACK, 4);
}


// a tiny home-made random number generator (good enough to place a target)
static uint16_t randomState = 0xACE1;
static uint16_t randomNext(void) {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    randomState ^= randomState << 8;
    return randomState;
}


// Light one dot at screen position (x, y), where x = 0 is the far-left column
// and x = 31 the far-right. This is the single place that knows how our screen
// coordinates map onto the physically-chained panels, so the MIRROR_X / FLIP_Y
// switches above fix any wiring flip without touching the rest of the code.
static void plot(uint8_t grid[8][4], uint8_t x, uint8_t y) {
    if (x >= COLS || y >= ROWS) return;
    uint8_t panel = x / 8;                      // which of the 4 panels
    uint8_t bit   = x % 8;                       // which LED inside that panel
    if (REVERSE_PANELS) panel = 3 - panel;
    if (REVERSE_BITS)   bit   = 7 - bit;
    if (FLIP_Y)         y     = (ROWS - 1) - y;
    grid[y][panel] |= (1 << bit);
}


// game state
static uint8_t  aimX, aimY;         // where the dot is (screen columns/rows)
static uint8_t  targetX, targetY;   // top-left corner of the 2x2 target
static uint16_t score;
static uint8_t  lockedOn;           // is the dot touching the target?

// drop a new target somewhere legal (the whole 2x2 stays between the walls)
static void newTarget(void) {
    targetX = LEFT_EDGE + (randomNext() % (RIGHT_EDGE - LEFT_EDGE));   // 3..27
    targetY = randomNext() % (ROWS - 1);                              // 0..6
}

// locked when the dot is on any of the four cells of the 2x2 target
static uint8_t checkLock(void) {
    return (aimX == targetX || aimX == targetX + 1) &&
           (aimY == targetY || aimY == targetY + 1);
}


// main
int main(void) {
    DDRD  |= (1 << GREEN_LED) | (1 << RED_LED);     // LEDs out
    DDRD  &= ~(1 << BUTTON);                         // button in
    PORTD |= (1 << BUTTON);                          // with pull-up (reads 1 until pressed)

    setupSerial();
    ADC_init();
    matrixSetupPins();
    matrixInit();
    setupServos();
    sei();

    tft_init();
    tft_fill_screen(TFT_BLUE);                       // quick "screen is alive" flash
    _delay_ms(250);
    tft_fill_screen(TFT_BLACK);
    tft_draw_text(8, 10, "SCORE", TFT_WHITE, TFT_BLACK, 2);

    randomState ^= (uint16_t)(ADC_read(0) << 6) ^ (uint16_t)ADC_read(1);  // stir in joystick noise
    if (randomState == 0) randomState = 0xBEEF;
    newTarget();
    score = 0;
    showScore(score);
    serialText("\r\nTurretLock - aim with joystick, press to fire.\r\n");

    uint8_t  grid[8][4];
    int16_t  fineX = ((RIGHT_EDGE - LEFT_EDGE) * FINE) / 2;   // dot starts centered
    int16_t  fineY = ((ROWS - 1) * FINE) / 2;
    uint8_t  buttonLast = 1;
    uint8_t  frame = 0;

    while (1) {
        // joystick as a speed: push moves the dot, releasing leaves it put
        int16_t pushX = (int16_t)ADC_read(0) - 512;
        int16_t pushY = (int16_t)ADC_read(1) - 512;
        if (pushX > -JOY_DEADZONE && pushX < JOY_DEADZONE) pushX = 0;
        if (pushY > -JOY_DEADZONE && pushY < JOY_DEADZONE) pushY = 0;

        fineX += pushX / JOY_SPEED;
        fineY -= pushY / JOY_SPEED;                  // minus so pushing up moves up

        if (fineX < 0) fineX = 0;
        if (fineX > (RIGHT_EDGE - LEFT_EDGE) * FINE) fineX = (RIGHT_EDGE - LEFT_EDGE) * FINE;
        if (fineY < 0) fineY = 0;
        if (fineY > (ROWS - 1) * FINE) fineY = (ROWS - 1) * FINE;

        aimX = LEFT_EDGE + (uint8_t)(fineX / FINE);  // always stays between the walls
        aimY = (uint8_t)(fineY / FINE);

        // point the turret at the dot
        OCR1A = (uint16_t)map_value(LEFT_EDGE, RIGHT_EDGE, SERVO_LEFT,  SERVO_RIGHT, aimX);
        OCR1B = (uint16_t)map_value(0, ROWS - 1,          SERVO_RIGHT, SERVO_LEFT,  aimY);

        lockedOn = checkLock();

        // fire: only on the press (not while held), and only if we're locked
        uint8_t button = (PIND >> BUTTON) & 1;
        if (buttonLast == 1 && button == 0 && lockedOn) {
            score++;
            showScore(score);
            newTarget();
        }
        buttonLast = button;

        // LEDs: locked -> green on, red off. not locked -> green off, red blinks.
        uint8_t blink = (frame >> 4) & 1;
        if (lockedOn) {
            PORTD |=  (1 << GREEN_LED);
            PORTD &= ~(1 << RED_LED);
        } else {
            PORTD &= ~(1 << GREEN_LED);
            if (blink) PORTD |=  (1 << RED_LED);
            else       PORTD &= ~(1 << RED_LED);
        }

        // every so often re-send the matrix config, so a chip that got glitched
        // by a noisy/loose wire (e.g. stuck all-on) heals itself
        if ((frame & 0x1F) == 0) matrixRefreshConfig();

        // draw walls, target, then the dot, and push it to the matrix
        for (uint8_t r = 0; r < 8; r++)
            for (uint8_t p = 0; p < 4; p++)
                grid[r][p] = 0;
        for (uint8_t y = 0; y < ROWS; y++)
            for (uint8_t w = 0; w < WALL_W; w++) {
                plot(grid, w, y);                    // left wall
                plot(grid, COLS - 1 - w, y);         // right wall
            }
        plot(grid, targetX,     targetY);
        plot(grid, targetX + 1, targetY);
        plot(grid, targetX,     targetY + 1);
        plot(grid, targetX + 1, targetY + 1);
        plot(grid, aimX, aimY);
        matrixDraw(grid);

        frame++;
        _delay_ms(15);
    }

    return 0;
}
