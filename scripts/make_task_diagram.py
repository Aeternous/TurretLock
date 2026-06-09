import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, FancyArrowPatch, Polygon

fig, ax = plt.subplots(figsize=(18, 11))
ax.set_xlim(0, 18); ax.set_ylim(0, 11); ax.axis('off')

# ---- helpers ----
def task_box(cx, cy, w, h, name, period):
    ax.add_patch(Rectangle((cx-w/2, cy-h/2), w, h, fill=False, lw=2))
    ax.text(cx, cy, name, ha='center', va='center', fontsize=12, fontweight='bold')
    ax.text(cx, cy-h/2-0.32, f"Period = {period}", ha='center', va='top',
            fontsize=10.5, family='sans-serif')

def mux(cx, cy, w, h, name, period):
    # trapezoid pointing right
    pts = [(cx-w/2, cy+h/2), (cx+w/2, cy+h/2*0.55),
           (cx+w/2, cy-h/2*0.55), (cx-w/2, cy-h/2)]
    ax.add_patch(Polygon(pts, closed=True, fill=True, facecolor='#bdbdbd',
                 edgecolor='k', lw=2))
    ax.text(cx-0.1, cy+0.15, name, ha='center', va='center', fontsize=10.5, fontweight='bold')
    ax.text(cx-0.1, cy-0.35, f"(Period = {period})", ha='center', va='center',
            fontsize=9, family='sans-serif')
    return (cx-w/2, cy)  # left side

def pill(cx, cy, label):
    w = 0.35 + 0.10*len(label)
    ax.add_patch(FancyBboxPatch((cx-w/2, cy-0.27), w, 0.54,
                 boxstyle="round,pad=0.02,rounding_size=0.27",
                 facecolor='#d0d0d0', edgecolor='k', lw=1.2))
    ax.text(cx, cy, label, ha='center', va='center', fontsize=9.5, family='sans-serif')
    return (cx-w/2, cy, cx+w/2)  # left, y, right

def arrow(p1, p2, color='k', lw=1.3):
    ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle='-|>', mutation_scale=14,
                 lw=lw, color=color, zorder=2))

def line(x1, y1, x2, y2, color='k'):
    ax.plot([x1, x2], [y1, y2], color=color, lw=1.3, zorder=1)


# ---- input pins (left edge) ----
inputs = [("A0", 8.4), ("A1", 8.0), ("D2", 7.6)]
for nm, yy in inputs:
    ax.text(0.4, yy, nm, ha='left', va='center', fontsize=12, family='sans-serif')
    line(1.0, yy, 3.0, yy)
    arrow((2.8, yy), (3.0, yy))

# ---- Task 1: Joystick (writes aimX/Y, firePressed, clickDone) ----
task_box(4.0, 8.0, 2.0, 1.7, "JOYSTICK", "20 ms")
# pills written by joystick
pAim   = pill(6.4, 8.6, "aimXY")
pFire  = pill(6.4, 8.0, "firePressed")
pClick = pill(6.4, 7.4, "clickDone")
arrow((5.0, pAim[1]), (pAim[0], pAim[1]))
arrow((5.0, pFire[1]), (pFire[0], pFire[1]))
arrow((5.0, pClick[1]), (pClick[0], pClick[1]))

# ---- Task 2: Game (reads firePressed/clickDone, writes targetX/Y, score/lives/highScore, gameState, lockedOn) ----
task_box(9.0, 6.0, 2.0, 1.7, "GAME", "20 ms")
# pills feeding into Game (firePressed, clickDone, aimXY)
arrow((pFire[2], pFire[1]), (8.0, 6.4))
arrow((pClick[2], pClick[1]), (8.0, 6.2))
# game writes these pills
pState = pill(11.6, 8.5, "gameState")
pTgt   = pill(11.6, 7.7, "targetXY")
pScore = pill(11.6, 5.5, "score")
pLives = pill(11.6, 4.8, "lives")
pHigh  = pill(11.6, 4.1, "highScore")
pLock  = pill(11.6, 3.4, "lockedOn")
for p in (pState, pTgt, pScore, pLives, pHigh, pLock):
    sy = max(min(p[1], 6.6), 5.4)
    arrow((10.0, sy), (p[0], p[1]))

# ---- Output MUXes (one per peripheral) ----
mMat = mux(15.4, 8.7, 1.8, 1.5, "MATRIX MUX", "30 ms")
mSrv = mux(15.4, 6.3, 1.8, 1.3, "SERVO MUX",  "40 ms")
mHud = mux(15.4, 4.5, 1.8, 1.4, "HUD MUX",    "60 ms")
mLed = mux(15.4, 2.7, 1.8, 1.4, "LED MUX",    "150 ms")

# ---- aim also feeds Matrix and Servo (top wraparound, like 'A' in the TRANSMIT example) ----
line(pAim[0], pAim[1]+0.27, pAim[0], 10.4)        # up from aimXY pill
line(pAim[0], 10.4, 14.45, 10.4)                  # across the top
line(14.45, 10.4, 14.45, 5.95)                    # down to servo level
arrow((14.45, 9.0), (mMat[0], 9.0))               # tap into Matrix MUX
arrow((14.45, 5.95), (mSrv[0], 5.95))             # tap into Servo MUX

# pill -> MUX edges
arrow((pState[2], pState[1]), (mMat[0], 8.9))
arrow((pTgt[2], pTgt[1]),     (mMat[0], 8.4))
arrow((pScore[2], pScore[1]), (mHud[0], 4.7))
arrow((pLives[2], pLives[1]), (mHud[0], 4.5))
arrow((pHigh[2],  pHigh[1]),  (mHud[0], 4.3))
arrow((pState[2], pState[1]), (mHud[0], 4.9))
arrow((pLock[2],  pLock[1]),  (mLed[0], 2.85))
arrow((pState[2], pState[1]), (mLed[0], 2.6))

# ---- Output lines on right ----
for cy, lbl in [(8.7, "B1: LED Matrix"), (6.3, "B2: Servos"),
                (4.5, "B3: TFT"),       (2.7, "B4: LEDs")]:
    arrow((16.3, cy), (16.9, cy))
    ax.text(16.95, cy, lbl, ha='left', va='center', fontsize=10, family='sans-serif')

# ---- variable declarations (bottom left, like the example) ----
decls = (
    "unsigned char aimX, aimY;\n"
    "unsigned char firePressed, clickDone;\n"
    "unsigned char gameState;\n"
    "unsigned char targetX, targetY;\n"
    "unsigned int  score;\n"
    "unsigned char lives;\n"
    "unsigned int  highScore;\n"
    "unsigned char lockedOn;"
)
ax.text(0.3, 1.7, decls, ha='left', va='top', fontsize=10.5, family='monospace')

plt.savefig('images/task_diagram.png', dpi=150, bbox_inches='tight')
print('wrote task_diagram.png')
