# TurretLock

A bare-metal AVR (Arduino Uno) game for a custom lab. No Arduino libraries —
everything is hand-written against the chip's registers.

## The game
The LED matrix is the playfield. Walls sit on the far left and far right (4
columns each), and a 2x2 target appears in the middle. You move an aim dot with
the joystick; when the dot touches the target you're "locked on" (green LED on,
red LED off). Press the joystick button while locked to score — the score
updates on the TFT screen, and a new target appears. Two servos act as a turret
that follows the dot.

The joystick is spring-loaded, so it controls the dot as a *speed* (push to
glide, release to hold) instead of a position — it never snaps back to center.

## Pin map (Arduino Uno)
| Pin | Use |
|-----|-----|
| A0 / A1 | Joystick X / Y |
| A2 / A3 | Servo X / Y signal |
| D2 | Joystick button |
| D4 / D5 | Green / Red LED |
| D7 / D8 / D9 | TFT RST / DC / CS |
| D10 | MAX7219 CS |
| D11 | MAX7219 DIN + TFT SDA (shared) |
| D13 | MAX7219 CLK + TFT SCK (shared) |

Servos get power from a separate 5V supply with a common ground.

## Source files
- `src/phari012_main.cpp` — game loop, servos, serial, score
- `src/phari012_max7219.cpp` / `include/phari012_max7219.h` — LED matrix driver
- `src/phari012_tft.cpp` / `include/phari012_tft.h` — ST7735 TFT driver + font
- `include/phari012_periph.h` — ADC (joystick), number scaling, bit helper

## Build & flash
Uses [PlatformIO](https://platformio.org/):
```
pio run            # compile
pio run -t upload  # compile + flash to the board
pio device monitor # watch the score over serial (9600 baud)
```

## Handy tuning knobs (in `src/phari012_main.cpp`)
- `WALL_W` — wall thickness in columns (4)
- `JOY_SPEED` — dot speed (bigger = slower)
- `JOY_DEADZONE` — how far to push before the dot moves
- `REVERSE_PANELS` / `REVERSE_BITS` / `FLIP_Y` — fix matrix orientation if a new
  wiring comes out mirrored or upside down
- TFT orientation: the `0x36` value in `tft_init()` (`src/phari012_tft.cpp`)

## Change log
Newest first. Grouped by file.

### Matrix mapping + gameplay rework
- `phari012_main.cpp`:
  - Walls set to 4 columns (matches proposal).
  - All matrix drawing goes through one `plot(x, y)` helper, where x=0 is the
    far-left column; `REVERSE_PANELS` / `REVERSE_BITS` / `FLIP_Y` switches correct
    any wiring flip in one place.
  - Joystick changed from absolute to speed control (push = glide, release =
    hold), so the dot no longer snaps back to center.
  - LEDs: locked = green solid + red off; not locked = red blink + green off.
  - Dot is hard-constrained to stay between the walls.
  - Periodically re-sends the matrix config (`matrixRefreshConfig`) so a chip
    glitched by a loose/noisy wire (e.g. stuck all-on) heals itself.
  - Trimmed unused/AI-ish locals; plain-English comments throughout.
- `phari012_max7219.*`:
  - Renamed to friendlier names: `SPI_SEND`->`sendByte`, `MAX_*`->`matrix*`.
  - Added `matrixRefreshConfig()` and call it from the game loop.
- `phari012_tft.cpp`: TFT flipped 180° (`0x36` value); uses `sendByte`.
- `phari012_periph.h`: humanized parameter names (`channel`, `fromLow/toHigh/
  value`, `original/bitNumber/on`).

### ST7735 TFT driver added
- `phari012_tft.cpp` / `phari012_tft.h` (new): from-scratch ST7735 driver — init
  sequence, bit-banged SPI sharing the matrix bus, a 5x7 PROGMEM font, and
  `tft_fill_rect` / `tft_fill_screen` / `tft_draw_char` / `tft_draw_text`.
- `phari012_main.cpp`: TFT init + blue "alive" flash + a static "SCORE" label;
  score now drawn in big cyan digits (still printed to serial too).

### Submission housekeeping
- Renamed `*.c` source files to `*.cpp` to match the lab's naming rule.

### Demo 2 base
- `phari012_main.cpp`: game loop, background-timer dual servos (Timer1), lock
  LEDs, serial score, random target spawner.
- `phari012_max7219.*`: bare-metal MAX7219 matrix driver (bit-banged SPI).
- `phari012_periph.h`: ADC joystick read, range mapping, bit helper.
