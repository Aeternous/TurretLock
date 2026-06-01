/*        Your Name & E-mail:
Pranae Harirajan, phari012@ucr.edu
 *        Discussion Section:
RB

 *        Assignment: Custom Laboratory Project - Demo Video #2

 *        Exercise Description: TurretLock game loop - matrix gameplay, dual servos, lock LEDs, TFT score

 *        I acknowledge all content contained herein, excluding template or example code, is my own original work.
yes
 */

// TurretLock - the LED matrix is the playfield. Walls sit on the far left and
// far right; a 2x2 target appears between them. You glide an aim dot with the
// joystick (push to move, let go to stay put). Land on the target and you lock
// on - the red LED stops, the green LED stays lit. Press the joystick button
// while locked to score: the number on the TFT goes up and a new target spawns.
// Two servos act as a turret that points wherever the dot is.

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "phari012_periph.h"
#include "phari012_max7219.h"
#include "phari012_tft.h"

// Pins
#define GREEN_LED  PD4    // D4
#define RED_LED    PD5    // D5
#define BUTTON     PD2    // D2  (joystick click: 0 when pressed)
#define SERVO_X    PC2    // A2
#define SERVO_Y    PC3    // A3

// Playfield: 32 columns wide, 8 rows tall, with a wall on each side
#define COLS        32
#define ROWS        8
#define WALL_W      4                      // wall is 4 columns thick (matches proposal)
#define LEFT_EDGE   WALL_W                 // first column you can be in   = 4
#define RIGHT_EDGE  (COLS - 1 - WALL_W)    // last column you can be in     = 27

// Matrix wiring fixups. The four panels are chained together, and how they're
// wired decides the orientation. Three switches sort it out:
//   REVERSE_PANELS - which physical end is panel 1 (left vs right)
//   REVERSE_BITS   - which way columns count inside each single panel
//   FLIP_Y         - top vs bottom
#define REVERSE_PANELS  1
#define REVERSE_BITS    0
#define FLIP_Y          0

// Joystick feel (it's spring-loaded, so we read it as a speed, not a position)
#define FINE          64    // sub-steps per cell, makes slow moves smooth
#define JOY_DEADZONE  90    // ignore small wiggle near center (out of ~512)
#define JOY_SPEED     28    // bigger = slower dot

// Servo pulse widths in Timer1 ticks (0.5us each): ~0.5ms / 1.5ms / 2.5ms
#define SERVO_LEFT    1000
#define SERVO_MID     3000
#define SERVO_RIGHT   5000

// Shared variables
unsigned char aimX = 0, aimY = 0;       // where the dot is (screen columns/rows)
unsigned char targetX = 0, targetY = 0; // top-left corner of the 2x2 target
unsigned int  score = 0;
unsigned char lockedOn = 0;             // is the dot touching the target?
unsigned int  randomState = 0xACE1;     // seed for the random target placement

// Servos: Timer1 makes the pulses in the background so the game never stalls.
// Every 20ms it raises both pins; OCR1A / OCR1B decide when each drops again,
// and that pulse width is the angle. main() just writes the angle it wants.
void setupServos() {
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

// Serial (so we can watch the score in the monitor)
void setupSerial() {
  UBRR0L = 103;                                  // 9600 baud at 16MHz
  UCSR0B = (1 << TXEN0);
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);         // 8N1
}
void serialText(const char *s) {
  while (*s) {
    while (!(UCSR0A & (1 << UDRE0))) {}
    UDR0 = *s++;
  }
}

// Turn a number like 42 into text "42"
void numberToText(unsigned int n, char *out) {
  char rev[6];
  unsigned char i = 0;
  if (n == 0) { out[0] = '0'; out[1] = '\0'; return; }
  while (n) { rev[i++] = '0' + (n % 10); n /= 10; }
  unsigned char j = 0;
  while (i--) { out[j++] = rev[i]; }
  out[j] = '\0';
}

// Show the score. "SCORE" is painted once in main(); here we only repaint the
// number (wiping its box first so 10 -> 9 doesn't leave the old digit behind).
void showScore(unsigned int sc) {
  char text[6];
  numberToText(sc, text);
  serialText("Score: "); serialText(text); serialText("\r\n");
  tft_fill_rect(8, 46, TFT_W - 16, 32, TFT_BLACK);
  tft_draw_text(8, 46, text, TFT_CYAN, TFT_BLACK, 4);
}

// A tiny home-made random number generator (good enough to place a target)
unsigned int randomNext() {
  randomState ^= randomState << 7;
  randomState ^= randomState >> 9;
  randomState ^= randomState << 8;
  return randomState;
}

// Light one dot at screen position (x, y), where x = 0 is the far-left column
// and x = 31 the far-right. This is the single place that knows how our screen
// coordinates map onto the chained panels, so the switches above fix any wiring
// flip without touching the rest of the code.
void plot(unsigned char grid[8][4], unsigned char x, unsigned char y) {
  if (x >= COLS || y >= ROWS) { return; }
  unsigned char panel = x / 8;                 // which of the 4 panels
  unsigned char bit   = x % 8;                  // which LED inside that panel
  if (REVERSE_PANELS) { panel = 3 - panel; }
  if (REVERSE_BITS)   { bit   = 7 - bit; }
  if (FLIP_Y)         { y     = (ROWS - 1) - y; }
  grid[y][panel] |= (1 << bit);
}

// Drop a new target somewhere legal (the whole 2x2 stays between the walls)
void newTarget() {
  targetX = LEFT_EDGE + (randomNext() % (RIGHT_EDGE - LEFT_EDGE));   // 4..26
  targetY = randomNext() % (ROWS - 1);                              // 0..6
}

// Locked when the dot is on any of the four cells of the 2x2 target
unsigned char checkLock() {
  return (aimX == targetX || aimX == targetX + 1) &&
         (aimY == targetY || aimY == targetY + 1);
}

int main(void) {
  DDRD  |= (1 << GREEN_LED) | (1 << RED_LED);     // LEDs out
  DDRD  &= ~(1 << BUTTON);                         // button in
  PORTD |= (1 << BUTTON);                          // pull-up (reads 1 until pressed)

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

  randomState ^= (unsigned int)(ADC_read(0) << 6) ^ (unsigned int)ADC_read(1);  // stir in joystick noise
  if (randomState == 0) { randomState = 0xBEEF; }
  newTarget();
  score = 0;
  showScore(score);
  serialText("\r\nTurretLock - aim with joystick, press to fire.\r\n");

  unsigned char grid[8][4];
  int fineX = ((RIGHT_EDGE - LEFT_EDGE) * FINE) / 2;   // dot's fine position, starts centered
  int fineY = ((ROWS - 1) * FINE) / 2;
  unsigned char buttonLast = 1;
  unsigned char frame = 0;

  while (1) {
    // Joystick as a speed: push moves the dot, releasing leaves it put
    int pushX = (int)ADC_read(0) - 512;
    int pushY = (int)ADC_read(1) - 512;
    if (pushX > -JOY_DEADZONE && pushX < JOY_DEADZONE) { pushX = 0; }
    if (pushY > -JOY_DEADZONE && pushY < JOY_DEADZONE) { pushY = 0; }

    fineX += pushX / JOY_SPEED;
    fineY -= pushY / JOY_SPEED;                  // minus so pushing up moves up

    if (fineX < 0) { fineX = 0; }
    if (fineX > (RIGHT_EDGE - LEFT_EDGE) * FINE) { fineX = (RIGHT_EDGE - LEFT_EDGE) * FINE; }
    if (fineY < 0) { fineY = 0; }
    if (fineY > (ROWS - 1) * FINE) { fineY = (ROWS - 1) * FINE; }

    aimX = LEFT_EDGE + (unsigned char)(fineX / FINE);  // always stays between the walls
    aimY = (unsigned char)(fineY / FINE);

    // Point the turret at the dot
    OCR1A = (unsigned int)map_value(LEFT_EDGE, RIGHT_EDGE, SERVO_LEFT,  SERVO_RIGHT, aimX);
    OCR1B = (unsigned int)map_value(0, ROWS - 1,          SERVO_RIGHT, SERVO_LEFT,  aimY);

    lockedOn = checkLock();

    // Fire: only on the press (not while held), and only if we're locked
    unsigned char button = (PIND >> BUTTON) & 1;
    if (buttonLast == 1 && button == 0 && lockedOn) {
      score++;
      showScore(score);
      newTarget();
    }
    buttonLast = button;

    // LEDs: locked -> green on, red off. not locked -> green off, red blinks.
    unsigned char blink = (frame >> 4) & 1;
    if (lockedOn) {
      PORTD |=  (1 << GREEN_LED);
      PORTD &= ~(1 << RED_LED);
    } else {
      PORTD &= ~(1 << GREEN_LED);
      if (blink) { PORTD |=  (1 << RED_LED); }
      else       { PORTD &= ~(1 << RED_LED); }
    }

    // Every so often re-send the matrix config, so a chip that got glitched by
    // a noisy/loose wire (e.g. stuck all-on) heals itself
    if ((frame & 0x1F) == 0) { matrixRefreshConfig(); }

    // Draw walls, target, then the dot, and push it to the matrix
    for (unsigned char r = 0; r < 8; r++) {
      for (unsigned char p = 0; p < 4; p++) {
        grid[r][p] = 0;
      }
    }
    for (unsigned char y = 0; y < ROWS; y++) {
      for (unsigned char w = 0; w < WALL_W; w++) {
        plot(grid, w, y);                    // left wall
        plot(grid, COLS - 1 - w, y);         // right wall
      }
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
