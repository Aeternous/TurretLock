/* Pranae Harirajan, phari012@ucr.edu
 * Discussion Section: RB
 * Assignment: Custom Laboratory Project - Final
 * I acknowledge all content contained herein, excluding template or example code, is my own original work. yes
 */

// TurretLock: aim a dot on the LED matrix, hit the target for 100 points, miss
// and lose a life. Built as Concurrent SynchSMs run by a task scheduler.

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "phari012_periph.h"
#include "phari012_max7219.h"
#include "phari012_tft.h"

// Pins
#define GREEN_LED  PD4    // D4
#define RED_LED    PD5    // D5
#define BUTTON     PD2    // D2  (joystick click: 0 when pressed)
#define SERVO_X    PC2    // A2
#define SERVO_Y    PC3    // A3

// Playfield: 32 columns, 8 rows, a wall on each side
#define COLS        32
#define ROWS        8
#define WALL_W      4
#define LEFT_EDGE   WALL_W                 // first playable column = 4
#define RIGHT_EDGE  (COLS - 1 - WALL_W)    // last playable column  = 27

// Matrix orientation switches (fix a mirrored/flipped picture without rewiring)
#define REVERSE_PANELS  1
#define REVERSE_BITS    0
#define FLIP_Y          0

// Joystick feel (spring-loaded, so it's a speed not a position)
#define FINE          64
#define JOY_DEADZONE  90
#define JOY_SPEED     28

// Servo pulse widths in Timer1 ticks (0.5us each)
#define SERVO_LEFT    1000
#define SERVO_MID     3000
#define SERVO_RIGHT   5000

// Game settings
#define LIVES_START   5
#define POINTS        100
#define DBL_WINDOW    400   // ms window to tell a single click from a double click

// Where the high score lives in EEPROM
#define EE_HIGH  ((uint16_t*)0)

// Task scheduler
typedef struct _task {
  signed char state;
  unsigned long period;
  unsigned long elapsedTime;
  int (*TickFct)(int);
} task;

#define NUM_TASKS  6
task tasks[NUM_TASKS];

const unsigned long GCD_PERIOD   = 1;    // scheduler tick = 1ms
const unsigned long PERIOD_JOY   = 20;
const unsigned long PERIOD_GAME  = 20;
const unsigned long PERIOD_MATRIX = 30;
const unsigned long PERIOD_SERVO = 40;
const unsigned long PERIOD_HUD   = 60;
const unsigned long PERIOD_LED   = 150;

// Shared variables
unsigned char gameState  = 0;    // mirror of the game task's state
int           fineX = 0, fineY = 0;
unsigned char aimX = 0, aimY = 0;
unsigned char targetX = 0, targetY = 0;
unsigned int  score = 0;
unsigned char lives = LIVES_START;
unsigned int  highScore = 0;
unsigned char lockedOn = 0;
unsigned char firePressed = 0;   // press edge, used for shooting
unsigned char clickDone = 0;     // a tap finished (used by start and game over)
unsigned char buttonLast = 1;
unsigned int  randomState = 0xACE1;

// Game states (ST_RESET is the "Reset High Score?" prompt)
enum GameStates { ST_START, ST_PLAY, ST_OVER, ST_RESET };

// Timer2 raises a flag every 1ms; the main loop runs the due tasks
volatile unsigned char schedulerFlag = 0;
void schedulerInit() {
  TCCR2A = (1 << WGM21);              // CTC mode
  TCCR2B = (1 << CS22);               // prescaler 64
  OCR2A  = 249;                       // 16MHz / 64 / 250 = 1000 Hz = 1ms
  TIMSK2 = (1 << OCIE2A);
}
ISR(TIMER2_COMPA_vect) {
  schedulerFlag = 1;
}

// Timer1 makes the servo pulses; OCR1A/OCR1B set each pin's high time (= angle)
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
ISR(TIMER1_OVF_vect) {
  PORTC |= (1 << SERVO_X) | (1 << SERVO_Y);
}
ISR(TIMER1_COMPA_vect) {
  PORTC &= ~(1 << SERVO_X);
}
ISR(TIMER1_COMPB_vect) {
  PORTC &= ~(1 << SERVO_Y);
}

// Turn a number into text, e.g. 42 -> "42"
void numberToText(unsigned int n, char *out) {
  char rev[6];
  unsigned char i = 0;
  if (n == 0) {
    out[0] = '0';
    out[1] = '\0';
    return;
  }
  while (n) {
    rev[i++] = '0' + (n % 10);
    n /= 10;
  }
  unsigned char j = 0;
  while (i--) {
    out[j++] = rev[i];
  }
  out[j] = '\0';
}

// Tiny random generator, good enough to place a target
unsigned int randomNext() {
  randomState ^= randomState << 7;
  randomState ^= randomState >> 9;
  randomState ^= randomState << 8;
  return randomState;
}

// Drop a new target somewhere legal (the 2x2 stays between the walls)
void newTarget() {
  targetX = LEFT_EDGE + (randomNext() % (RIGHT_EDGE - LEFT_EDGE));
  targetY = randomNext() % (ROWS - 1);
}

// Locked when the dot is on any of the four cells of the 2x2 target
unsigned char checkLock() {
  return (aimX == targetX || aimX == targetX + 1) &&
         (aimY == targetY || aimY == targetY + 1);
}

// Start a fresh round: reset score and lives, center the dot, spawn a target
void startNewGame() {
  score = 0;
  lives = LIVES_START;
  fineX = ((RIGHT_EDGE - LEFT_EDGE) * FINE) / 2;
  fineY = ((ROWS - 1) * FINE) / 2;
  lockedOn = 0;
  newTarget();
}

// Light one dot at (x, y); maps screen coords onto the chained panels
void plot(unsigned char grid[8][4], unsigned char x, unsigned char y) {
  if (x >= COLS || y >= ROWS) {
    return;
  }
  unsigned char panel = x / 8;
  unsigned char bit   = x % 8;
  if (REVERSE_PANELS) {
    panel = 3 - panel;
  }
  if (REVERSE_BITS) {
    bit = 7 - bit;
  }
  if (FLIP_Y) {
    y = (ROWS - 1) - y;
  }
  grid[y][panel] |= (1 << bit);
}

// Task 1: read the joystick into the aim (speed control) and catch fire presses
enum JoyStates { JOY_RUN };
int TickJoystick(int state) {
  int pushX = (int)ADC_read(0) - 512;
  int pushY = (int)ADC_read(1) - 512;
  if (pushX > -JOY_DEADZONE && pushX < JOY_DEADZONE) {
    pushX = 0;
  }
  if (pushY > -JOY_DEADZONE && pushY < JOY_DEADZONE) {
    pushY = 0;
  }

  fineX += pushX / JOY_SPEED;
  fineY -= pushY / JOY_SPEED;

  if (fineX < 0) {
    fineX = 0;
  }
  if (fineX > (RIGHT_EDGE - LEFT_EDGE) * FINE) {
    fineX = (RIGHT_EDGE - LEFT_EDGE) * FINE;
  }
  if (fineY < 0) {
    fineY = 0;
  }
  if (fineY > (ROWS - 1) * FINE) {
    fineY = (ROWS - 1) * FINE;
  }

  aimX = LEFT_EDGE + (unsigned char)(fineX / FINE);
  aimY = (unsigned char)(fineY / FINE);

  // Button: a press edge fires instantly. The release is debounced (must read
  // released for ~40 ms) so a noisy contact still registers one clean tap.
  static unsigned char releaseCount = 0;
  unsigned char b = (PIND >> BUTTON) & 1;   // 1 released, 0 pressed
  if (b == 0) {                             // pressed
    if (buttonLast == 1) {
      firePressed = 1;                      // press edge
    }
    buttonLast = 0;
    releaseCount = 0;
  } else if (buttonLast == 0) {             // looks released - confirm it
    releaseCount++;
    if (releaseCount >= 2) {
      clickDone = 1;                        // a tap finished
      buttonLast = 1;
    }
  }
  return state;
}

// Task 2: the game state machine (start -> play -> game over, + reset prompt).
// Start: a tap starts a game. Game over: a single click opens the reset prompt,
// a double click starts a new game. Play uses the raw edge so shooting is instant.
int TickGame(int state) {
  static unsigned int  resetTimer = 0;
  static unsigned int  overSettle = 0;
  static unsigned char overClicks = 0;
  static unsigned int  overClickTimer = 0;
  switch (state) {
    case ST_START:
      firePressed = 0;
      if (clickDone) {                 // a tap starts a game (no reset from here)
        clickDone = 0;
        startNewGame();
        state = ST_PLAY;
      }
      break;

    case ST_PLAY:
      clickDone = 0;                   // taps don't matter while playing
      lockedOn = checkLock();
      if (firePressed) {
        firePressed = 0;
        if (lockedOn) {
          score += POINTS;
          newTarget();
        } else {
          if (lives > 0) {
            lives--;
          }
          if (lives == 0) {
            if (score > highScore) {
              highScore = score;
              eeprom_update_word(EE_HIGH, highScore);   // save the new record
            }
            overSettle = 0;
            state = ST_OVER;
          }
        }
      }
      break;

    case ST_OVER:
      firePressed = 0;
      if (overSettle < 600) {
        overSettle += PERIOD_GAME;     // ignore the fatal shot for a moment
        clickDone = 0;
        overClicks = 0;
      } else {
        if (clickDone) {               // count taps to tell single from double
          clickDone = 0;
          overClicks++;
          overClickTimer = 0;
        }
        if (overClicks > 0) {
          overClickTimer += PERIOD_GAME;
          if (overClicks >= 2) {       // double click -> new game
            overClicks = 0;
            state = ST_START;
          } else if (overClickTimer >= DBL_WINDOW) {
            overClicks = 0;            // single click -> reset prompt
            resetTimer = 0;
            state = ST_RESET;
          }
        }
      }
      break;

    case ST_RESET:
      firePressed = 0;
      resetTimer += PERIOD_GAME;
      if (clickDone) {                 // a tap confirms the wipe
        clickDone = 0;
        highScore = 0;
        eeprom_update_word(EE_HIGH, 0);
        state = ST_START;
      } else if (resetTimer >= 4000) { // no tap for 4s = cancel
        state = ST_START;
      }
      break;

    default:
      state = ST_START;
      break;
  }
  gameState = state;
  return state;
}

// Task 3: draw the playfield (walls, target, dot) on the LED matrix
enum MatrixStates { MAT_RUN };
int TickMatrix(int state) {
  unsigned char grid[8][4];
  for (unsigned char r = 0; r < 8; r++) {
    for (unsigned char p = 0; p < 4; p++) {
      grid[r][p] = 0;
    }
  }
  if (gameState == ST_PLAY) {
    for (unsigned char y = 0; y < ROWS; y++) {
      for (unsigned char w = 0; w < WALL_W; w++) {
        plot(grid, w, y);
        plot(grid, COLS - 1 - w, y);
      }
    }
    plot(grid, targetX,     targetY);
    plot(grid, targetX + 1, targetY);
    plot(grid, targetX,     targetY + 1);
    plot(grid, targetX + 1, targetY + 1);
    plot(grid, aimX, aimY);
  }
  matrixDraw(grid);
  return state;
}

// Task 4: point the servos at wherever the dot is
enum ServoStates { SRV_RUN };
int TickServo(int state) {
  OCR1A = (unsigned int)map_value(LEFT_EDGE, RIGHT_EDGE, SERVO_LEFT,  SERVO_RIGHT, aimX);
  OCR1B = (unsigned int)map_value(0, ROWS - 1,          SERVO_RIGHT, SERVO_LEFT,  aimY);
  return state;
}

// Task 5: the TFT screen - start screen, live score + hearts, game over
enum HudStates { HUD_RUN };
int TickHUD(int state) {
  static unsigned char prevState = 0xFF;
  static unsigned int  prevScore = 0xFFFF;
  static unsigned char prevLives = 0xFF;
  char text[12];

  // On a state change, repaint the whole screen for that screen
  if (gameState != prevState) {
    tft_fill_screen(TFT_BLACK);
    if (gameState == ST_START) {
      tft_draw_text_centered(45, "PLAY GAME", TFT_WHITE, TFT_BLACK, 2);
      tft_draw_text_centered(75, "PRESS START", TFT_CYAN, TFT_BLACK, 2);
      //tft_draw_text_centered(120, "HI", TFT_YELLOW, TFT_BLACK, 1);
      numberToText(highScore, text);
      tft_draw_text_centered(135, text, TFT_YELLOW, TFT_BLACK, 2);
    } else if (gameState == ST_PLAY) {
      tft_draw_text_centered(6, "SCORE", TFT_WHITE, TFT_BLACK, 1);
      prevScore = 0xFFFF;   // force the score and hearts to draw below
      prevLives = 0xFF;
    } else if (gameState == ST_OVER) {
      tft_draw_text_centered(6, "GAME OVER", TFT_RED, TFT_BLACK, 2);
      tft_draw_text_centered(30, "1 CLICK=RESET HI", TFT_YELLOW, TFT_BLACK, 1);
      tft_draw_text_centered(42, "2 CLICK=PLAY", TFT_CYAN, TFT_BLACK, 1);
      // Show the player's final score in the middle if they didn't beat the record
      if (score < highScore) {
        char sline[16];
        unsigned char p = 0;
        const char *lbl = "SCORE: ";
        while (lbl[p]) {
          sline[p] = lbl[p];
          p++;
        }
        char num[6];
        numberToText(score, num);
        unsigned char q = 0;
        while (num[q]) {
          sline[p++] = num[q++];
        }
        sline[p] = '\0';
        tft_draw_text_centered(64, sline, TFT_WHITE, TFT_BLACK, 2);
      }
      tft_draw_text_centered(94, "HIGH SCORE", TFT_WHITE, TFT_BLACK, 2);
      numberToText(highScore, text);
      tft_draw_text_centered(116, text, TFT_YELLOW, TFT_BLACK, 3);
    } else if (gameState == ST_RESET) {
      tft_draw_text_centered(35, "RESET", TFT_RED, TFT_BLACK, 2);
      tft_draw_text_centered(60, "HI SCORE?", TFT_RED, TFT_BLACK, 2);
      tft_draw_text_centered(105, "CLICK = YES", TFT_WHITE, TFT_BLACK, 1);
      tft_draw_text_centered(125, "WAIT = NO", TFT_CYAN, TFT_BLACK, 1);
    }
    prevState = gameState;
  }

  // During play, refresh the score number and the hearts only when they change
  if (gameState == ST_PLAY) {
    if (score != prevScore) {
      tft_fill_rect(0, 20, TFT_W, 24, TFT_BLACK);
      numberToText(score, text);
      tft_draw_text_centered(20, text, TFT_CYAN, TFT_BLACK, 3);
      prevScore = score;
    }
    if (lives != prevLives) {
      tft_fill_rect(0, 70, TFT_W, 20, TFT_BLACK);
      int width = lives * 18;
      int x = (TFT_W - width) / 2;
      if (x < 0) {
        x = 0;
      }
      for (unsigned char k = 0; k < lives; k++) {
        tft_draw_heart(x + k * 18, 72, 2, TFT_RED);
      }
      prevLives = lives;
    }
  }
  return state;
}

// Task 6: lock LEDs - green solid when on target, red blinking otherwise
enum LedStates { LED_RUN };
int TickLED(int state) {
  static unsigned char blink = 0;
  blink ^= 1;
  if (gameState == ST_PLAY) {
    if (lockedOn) {
      PORTD |=  (1 << GREEN_LED);
      PORTD &= ~(1 << RED_LED);
    } else {
      PORTD &= ~(1 << GREEN_LED);
      if (blink) {
        PORTD |= (1 << RED_LED);
      } else {
        PORTD &= ~(1 << RED_LED);
      }
    }
  } else {
    PORTD &= ~((1 << GREEN_LED) | (1 << RED_LED));
  }
  return state;
}

int main(void) {
  DDRD  |= (1 << GREEN_LED) | (1 << RED_LED);
  DDRD  &= ~(1 << BUTTON);
  PORTD |= (1 << BUTTON);                          // pull-up

  ADC_init();
  matrixSetupPins();
  matrixInit();
  setupServos();
  tft_init();
  schedulerInit();
  sei();

  // Load the saved high score (a blank EEPROM reads 0xFFFF)
  highScore = eeprom_read_word(EE_HIGH);
  if (highScore == 0xFFFF) {
    highScore = 0;
  }
  randomState ^= (unsigned int)(ADC_read(0) << 6) ^ (unsigned int)ADC_read(1);
  if (randomState == 0) {
    randomState = 0xBEEF;
  }

  gameState = ST_START;

  unsigned char i = 0;
  tasks[i].state = JOY_RUN;  tasks[i].period = PERIOD_JOY;    tasks[i].elapsedTime = PERIOD_JOY;    tasks[i].TickFct = &TickJoystick; i++;
  tasks[i].state = ST_START; tasks[i].period = PERIOD_GAME;   tasks[i].elapsedTime = PERIOD_GAME;   tasks[i].TickFct = &TickGame;     i++;
  tasks[i].state = MAT_RUN;  tasks[i].period = PERIOD_MATRIX; tasks[i].elapsedTime = PERIOD_MATRIX; tasks[i].TickFct = &TickMatrix;   i++;
  tasks[i].state = SRV_RUN;  tasks[i].period = PERIOD_SERVO;  tasks[i].elapsedTime = PERIOD_SERVO;  tasks[i].TickFct = &TickServo;    i++;
  tasks[i].state = HUD_RUN;  tasks[i].period = PERIOD_HUD;    tasks[i].elapsedTime = PERIOD_HUD;    tasks[i].TickFct = &TickHUD;      i++;
  tasks[i].state = LED_RUN;  tasks[i].period = PERIOD_LED;    tasks[i].elapsedTime = PERIOD_LED;    tasks[i].TickFct = &TickLED;      i++;

  while (1) {
    while (!schedulerFlag) {}        // wait for the next 1ms tick
    schedulerFlag = 0;
    for (unsigned char t = 0; t < NUM_TASKS; t++) {
      if (tasks[t].elapsedTime == tasks[t].period) {
        tasks[t].state = tasks[t].TickFct(tasks[t].state);
        tasks[t].elapsedTime = 0;
      }
      tasks[t].elapsedTime += GCD_PERIOD;
    }
  }

  return 0;
}
