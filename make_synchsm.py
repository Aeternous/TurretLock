import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyArrowPatch
import math

def self_loop(ax, cx, cy, r, label="1", h=1.0, fs=13):
    dx = 0.45
    ys = cy + math.sqrt(max(r*r - dx*dx, 0)); ytop = cy + r + h
    ax.plot([cx+dx, cx+dx, cx-dx], [ys, ytop, ytop], color='k', lw=1.6)
    ax.add_patch(FancyArrowPatch((cx-dx, ytop), (cx-dx, ys),
                 arrowstyle='-|>', mutation_scale=15, lw=1.6, color='k'))
    ax.text(cx, ytop+0.12, label, ha='center', va='bottom', fontsize=fs)

def single(fname, title, period, locals_list, state, init_action, action):
    fig, ax = plt.subplots(figsize=(8.8, 9.4))
    ax.set_xlim(0, 10); ax.set_ylim(0, 12); ax.axis('off')
    ax.text(5, 11.6, title, ha='center', fontsize=16, fontweight='bold')
    y = 10.9
    ax.text(0.3, y, f"Period = {period}", ha='left', fontsize=12, family='monospace'); y -= 0.42
    for d in locals_list:
        ax.text(0.3, y, d, ha='left', fontsize=11, family='monospace'); y -= 0.40
    cx, cy, r = 5.0, 8.0, 1.0
    ax.add_patch(Circle((cx, cy), r, fill=False, lw=2))
    ax.text(cx, cy, state, ha='center', va='center', fontsize=12, fontweight='bold')
    ax.add_patch(FancyArrowPatch((cx-3.0, cy), (cx-r, cy),
                 arrowstyle='-|>', mutation_scale=18, lw=1.6, color='k'))
    if init_action:
        ax.text(cx-2.05, cy-0.6, init_action, ha='center', va='top', fontsize=10, family='monospace')
    self_loop(ax, cx, cy, r)
    ax.text(cx-2.3, cy-r-0.5, action, ha='left', va='top', fontsize=10.5, family='monospace')
    plt.savefig(fname, dpi=150, bbox_inches='tight'); plt.close()
    print('wrote', fname)

single('sm_joystick.png', "Joystick Task", "20 ms",
       ["unsigned char releaseCount;"], "READ",
       "releaseCount = 0;\nbuttonLast = 1;",
"""joyX = ADC_read(0);   joyY = ADC_read(1);
pushX = joyX - 512;   pushY = joyY - 512;
if (pushX>-DZ && pushX<DZ) pushX = 0;
if (pushY>-DZ && pushY<DZ) pushY = 0;
fineX += pushX / JOY_SPEED;
fineY -= pushY / JOY_SPEED;
clamp(fineX, 0, (RIGHT_EDGE-LEFT_EDGE)*FINE);
clamp(fineY, 0, (ROWS-1)*FINE);
aimX = LEFT_EDGE + fineX/FINE;
aimY = fineY/FINE;
b = (PIND>>BUTTON) & 1;
if (b == 0) {
    if (buttonLast==1) firePressed = 1;
    buttonLast = 0;  releaseCount = 0;
} else if (buttonLast==0) {
    if (++releaseCount >= 2) {
        clickDone = 1;  buttonLast = 1;
    }
}""")

single('sm_matrix.png', "Matrix Task", "30 ms",
       ["unsigned char grid[8][4];"], "DRAW", "",
"""for (r,p) grid[r][p] = 0;
if (gameState == ST_PLAY) {
    for (y=0; y<ROWS; y++)
      for (w=0; w<WALL_W; w++) {
        plot(grid, w, y);
        plot(grid, COLS-1-w, y);
      }
    plot(grid, targetX,   targetY);
    plot(grid, targetX+1, targetY);
    plot(grid, targetX,   targetY+1);
    plot(grid, targetX+1, targetY+1);
    plot(grid, aimX, aimY);
}
matrixDraw(grid);""")

single('sm_servo.png', "Servo Task", "40 ms", [], "RUN", "",
"""OCR1A = map_value(LEFT_EDGE, RIGHT_EDGE,
                  SERVO_LEFT, SERVO_RIGHT, aimX);
OCR1B = map_value(0, ROWS-1,
                  SERVO_RIGHT, SERVO_LEFT, aimY);""")

single('sm_hud.png', "HUD / TFT Task", "60 ms",
       ["unsigned char prevState, prevLives;", "unsigned int  prevScore;"], "RUN",
       "prevState = 0xFF;\nprevScore = 0xFFFF;\nprevLives = 0xFF;",
"""if (gameState != prevState) {
    drawScreen(gameState);
    prevState = gameState;
}
if (gameState == ST_PLAY) {
    if (score != prevScore) {
        drawScore(score);  prevScore = score;
    }
    if (lives != prevLives) {
        drawHearts(lives); prevLives = lives;
    }
}""")

single('sm_led.png', "LED Task", "150 ms",
       ["unsigned char blink;"], "RUN", "blink = 0;",
"""blink ^= 1;
if (gameState == ST_PLAY) {
    if (lockedOn) {
        GREEN_LED = 1;  RED_LED = 0;
    } else {
        GREEN_LED = 0;  RED_LED = blink;
    }
} else {
    GREEN_LED = 0;  RED_LED = 0;
}""")

# ---- Game state machine ----
def game():
    fig, ax = plt.subplots(figsize=(13, 10))
    ax.set_xlim(0, 13); ax.set_ylim(0, 12.5); ax.axis('off')
    ax.text(6.5, 12.1, "Game Task", ha='center', fontsize=16, fontweight='bold')
    ax.text(0.3, 11.4, "Period = 20 ms", ha='left', fontsize=12, family='monospace')
    ax.text(0.3, 11.0, "unsigned int  resetTimer, overSettle, overClickTimer;",
            ha='left', fontsize=10.5, family='monospace')
    ax.text(0.3, 10.65, "unsigned char overClicks;",
            ha='left', fontsize=10.5, family='monospace')

    S = {'START': (2.6, 8.0), 'PLAY': (9.0, 8.0), 'OVER': (9.0, 3.6), 'RESET': (2.6, 3.6)}
    r = 0.95
    for nm, (x, y) in S.items():
        ax.add_patch(Circle((x, y), r, fill=False, lw=2))
        ax.text(x, y, nm, ha='center', va='center', fontsize=10, fontweight='bold')

    def edge(a, b, label, rad=0.0, lx=None, ly=None):
        (x1, y1), (x2, y2) = S[a], S[b]
        ang = math.atan2(y2-y1, x2-x1)
        p1 = (x1 + r*math.cos(ang), y1 + r*math.sin(ang))
        p2 = (x2 - r*math.cos(ang), y2 - r*math.sin(ang))
        ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle='-|>', mutation_scale=14,
                     lw=1.5, color='k', connectionstyle=f"arc3,rad={rad}"))
        if lx is None: lx = (x1+x2)/2
        if ly is None: ly = (y1+y2)/2
        ax.text(lx, ly, label, ha='center', va='center', fontsize=8.5, family='monospace',
                bbox=dict(boxstyle='round,pad=0.18', fc='white', ec='none'))

    ax.add_patch(FancyArrowPatch((0.2, 8.0), (S['START'][0]-r, 8.0),
                 arrowstyle='-|>', mutation_scale=16, lw=1.6, color='k'))
    ax.text(1.0, 7.65, "(reset)", ha='center', va='top', fontsize=9, family='monospace')

    edge('START', 'PLAY', "clickDone /\nstartNewGame()", ly=8.4)
    edge('PLAY', 'OVER', "lives==0 /\nsave highScore\n(EEPROM)", lx=9.95)
    edge('OVER', 'RESET', "settle done &&\nsingle click", ly=3.1)
    edge('OVER', 'START', "double click", rad=0.16, lx=5.8, ly=6.4)
    edge('RESET', 'START', "clickDone / highScore=0,\nEEPROM   | 4s timeout", lx=1.5)
    for nm in S:
        x, y = S[nm]
        self_loop(ax, x, y, r, "else", h=0.6, fs=9)

    code = ("Per-tick state action (firePressed reset each tick):\n"
            "  PLAY:  lockedOn = checkLock();\n"
            "         if (firePressed) {\n"
            "             if (lockedOn) { score += 100; newTarget(); }\n"
            "             else if (--lives == 0) -> OVER\n"
            "         }\n"
            "  OVER:  if (overSettle < 600) overSettle += 20;\n"
            "         else count taps over DBL_WINDOW -> 1=RESET, 2=START\n"
            "  RESET: resetTimer += 20;  show 'RESET HI SCORE?'")
    ax.text(0.3, 1.7, code, ha='left', va='top', fontsize=9, family='monospace')
    plt.savefig('sm_game.png', dpi=150, bbox_inches='tight'); plt.close()
    print('wrote sm_game.png')

game()
