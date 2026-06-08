import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyArrowPatch, FancyBboxPatch

def self_loop(ax, cx, cy, r, label="1", h=1.0, fs=12):
    import math
    dx = 0.45
    ys = cy + math.sqrt(max(r*r - dx*dx, 0))
    ytop = cy + r + h
    ax.plot([cx+dx, cx+dx, cx-dx], [ys, ytop, ytop], color='k', lw=1.6)
    ax.add_patch(FancyArrowPatch((cx-dx, ytop), (cx-dx, ys),
                 arrowstyle='-|>', mutation_scale=15, lw=1.6, color='k'))
    ax.text(cx, ytop+0.15, label, ha='center', va='bottom', fontsize=fs)

def init_arrow(ax, cx, cy, r, action=None):
    ax.add_patch(FancyArrowPatch((cx-2.8, cy), (cx-r, cy),
                 arrowstyle='-|>', mutation_scale=18, lw=1.6, color='k'))
    if action:
        ax.text(cx-1.9, cy-0.35, action, ha='center', va='top', fontsize=10, family='monospace')

def header(ax, title, period, locals_txt):
    ax.text(5, 9.6, title, ha='center', fontsize=16, fontweight='bold')
    ax.text(0.3, 8.7, f"Period = {period}", ha='left', fontsize=11)
    if locals_txt:
        ax.text(0.3, 8.25, locals_txt, ha='left', fontsize=9.5, family='monospace')

def single(fname, title, period, locals_txt, state_name, actions):
    fig, ax = plt.subplots(figsize=(8, 6.2))
    ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.axis('off')
    header(ax, title, period, locals_txt)
    cx, cy, r = 5.2, 5.4, 1.05
    ax.add_patch(Circle((cx, cy), r, fill=False, lw=2))
    ax.text(cx, cy, state_name, ha='center', va='center', fontsize=11, fontweight='bold')
    init_arrow(ax, cx, cy, r)
    self_loop(ax, cx, cy, r)
    ax.text(cx-1.7, cy-r-0.55, "\n".join(actions), ha='left', va='top',
            fontsize=10.5, family='monospace')
    plt.savefig(fname, dpi=150, bbox_inches='tight'); plt.close()
    print('wrote', fname)

# ---- single-state tasks ----
single('sm_joystick.png', "T1 - Joystick", "20 ms", "local: releaseCount : uchar",
       "READ",
       ["joyX = ADC(0); joyY = ADC(1)",
        "deadzone -> fineX,fineY  (speed)",
        "clamp -> aimX, aimY",
        "press edge      -> firePressed",
        "debounced release -> clickDone"])

single('sm_matrix.png', "T3 - Matrix", "30 ms", "local: grid[8][4] : uchar",
       "DRAW",
       ["clear grid",
        "if gameState == PLAY:",
        "    draw walls, 2x2 target, aim dot",
        "render grid -> MAX7219 matrix"])

single('sm_servo.png', "T4 - Servo", "40 ms", "",
       "RUN",
       ["OCR1A = map(aimX -> angle)",
        "OCR1B = map(aimY -> angle)"])

single('sm_hud.png', "T5 - HUD / TFT", "60 ms",
       "local: prevState:uchar, prevScore:uint, prevLives:uchar",
       "RUN",
       ["if gameState changed: repaint screen",
        "if PLAY and score changed: redraw number",
        "if PLAY and lives changed: redraw hearts"])

single('sm_led.png', "T6 - LEDs", "150 ms", "local: blink : uchar",
       "RUN",
       ["if gameState == PLAY:",
        "    locked  -> green on, red off",
        "    unlocked-> green off, red blink",
        "else: both LEDs off"])

# ---- Game state machine (4 states) ----
def game():
    fig, ax = plt.subplots(figsize=(11.5, 9.3))
    ax.set_xlim(0, 12); ax.set_ylim(0, 11.6); ax.axis('off')
    ax.text(6, 11.2, "T2 - Game", ha='center', fontsize=16, fontweight='bold')
    ax.text(0.3, 10.5, "Period = 20 ms", ha='left', fontsize=11)
    ax.text(0.3, 10.1, "local: resetTimer, overSettle, overClicks, overClickTimer",
            ha='left', fontsize=9.5, family='monospace')

    S = {'START': (2.4, 7.3), 'PLAY': (8.2, 7.3), 'OVER': (8.2, 3.2), 'RESET': (2.4, 3.2)}
    r = 0.95
    for nm, (x, y) in S.items():
        ax.add_patch(Circle((x, y), r, fill=False, lw=2))
        ax.text(x, y, nm, ha='center', va='center', fontsize=10, fontweight='bold')

    def edge(a, b, label, rad=0.0, lx=None, ly=None):
        import math
        (x1, y1), (x2, y2) = S[a], S[b]
        ang = math.atan2(y2-y1, x2-x1)
        p1 = (x1 + r*math.cos(ang), y1 + r*math.sin(ang))
        p2 = (x2 - r*math.cos(ang), y2 - r*math.sin(ang))
        ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle='-|>', mutation_scale=14,
                     lw=1.5, color='k', connectionstyle=f"arc3,rad={rad}"))
        if lx is None: lx = (x1+x2)/2
        if ly is None: ly = (y1+y2)/2
        ax.text(lx, ly, label, ha='center', va='center', fontsize=8.5,
                family='monospace', bbox=dict(boxstyle='round,pad=0.15', fc='white', ec='none'))

    # init
    ax.add_patch(FancyArrowPatch((0.2, 7.3), (S['START'][0]-r, 7.3),
                 arrowstyle='-|>', mutation_scale=16, lw=1.6, color='k'))
    edge('START', 'PLAY', "clickDone /\nstartNewGame()", ly=7.65)
    edge('PLAY', 'OVER', "lives==0 /\nsave highScore", lx=8.95)
    edge('OVER', 'RESET', "single click", ly=2.75)
    edge('OVER', 'START', "double click", rad=0.18, lx=5.3, ly=5.9)
    edge('RESET', 'START', "tap / wipe EEPROM\nor 4s timeout", lx=1.35, ly=5.5)
    for nm in S:
        x, y = S[nm]
        self_loop(ax, x, y, r, "else", h=0.65, fs=9)

    actions = ("State actions:\n"
               "  START: wait for a tap -> start a game\n"
               "  PLAY : on fire, locked -> score+=100, newTarget; miss -> lives--\n"
               "  OVER : ignore input 600 ms; single click -> RESET, double -> START\n"
               "  RESET: 'RESET HI SCORE?'  tap=YES (highScore=0, EEPROM), wait 4s=NO")
    ax.text(0.3, 1.5, actions, ha='left', va='top', fontsize=9.5, family='monospace')
    plt.savefig('sm_game.png', dpi=150, bbox_inches='tight'); plt.close()
    print('wrote sm_game.png')

game()
