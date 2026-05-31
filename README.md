# TurretLock

A bare-metal AVR (Arduino Uno) game for a custom lab. No Arduino libraries —
everything is hand-written against the chip's registers.

## The game
The LED matrix is the playfield. Walls sit on the far left and far right, and a
2x2 target appears in the middle. You move an aim dot with the joystick; when the
dot touches the target you're "locked on" (red LED solid, green LED flashes).
Press the joystick button while locked to score — the score updates on the TFT
screen, and a new target appears. Two servos act as a turret that follows the dot.

The joystick is spring-loaded, so it controls the dot as a *speed* (push to glide,
release to hold) instead of a position — it never snaps back to center.

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
- `src/phari012_max7219.c` / `include/phari012_max7219.h` — LED matrix driver
- `src/phari012_tft.c` / `include/phari012_tft.h` — ST7735 TFT driver + font
- `include/phari012_periph.h` — ADC (joystick), number scaling, bit helper

## Build & flash
Uses [PlatformIO](https://platformio.org/):
```
pio run            # compile
pio run -t upload  # compile + flash to the board
pio device monitor # watch the score over serial (9600 baud)
```

## Handy tuning knobs (in `src/phari012_main.cpp`)
- `JOY_SPEED_DIV` — dot speed (bigger = slower)
- `JOY_DEADZONE` — how far to push before the dot moves
- TFT orientation: `MADCTL` value in `tft_init()` (`src/phari012_tft.c`)
