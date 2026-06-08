import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, FancyArrowPatch

fig, ax = plt.subplots(figsize=(16, 9))
ax.set_xlim(0, 16); ax.set_ylim(0, 9); ax.axis('off')

# header: system period + shared variable declarations
ax.text(0.2, 8.75, "Period = 1 ms", fontsize=12, family='monospace')
ax.text(0.2, 8.35,
        "unsigned char aimX, aimY, targetX, targetY, lives, lockedOn, firePressed, clickDone, gameState;",
        fontsize=9.5, family='monospace')
ax.text(0.2, 8.02, "unsigned int  score, highScore;   int fineX, fineY;",
        fontsize=9.5, family='monospace')

def task(x, y, w, h, name, period):
    ax.add_patch(Rectangle((x, y), w, h, fill=False, lw=1.7))
    ax.text(x+w/2, y+h*0.60, name, ha='center', va='center', fontsize=11, fontweight='bold')
    ax.text(x+w/2, y+h*0.30, f"(Period = {period})", ha='center', va='center', fontsize=8.5)
    return (x, y, w, h)

def pill(cx, cy, label):
    w = 0.30 + 0.085*len(label)
    ax.add_patch(FancyBboxPatch((cx-w/2, cy-0.25), w, 0.5,
                 boxstyle="round,pad=0.02,rounding_size=0.25", fill=True,
                 facecolor='white', edgecolor='k', lw=1.2, zorder=3))
    ax.text(cx, cy, label, ha='center', va='center', fontsize=8, zorder=4)
    return (cx-w/2, cy, cx+w/2, cy)  # left point, right point

def arr(p1, p2, color='k'):
    ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle='-|>', mutation_scale=12,
                 lw=1.2, color=color, zorder=2))

def hline(x1, x2, y):
    ax.plot([x1, x2], [y, y], color='k', lw=1.2, zorder=1)

# ---- inputs (pin lines on the left) ----
for nm, yy in [("A0 (VRx)", 6.7), ("A1 (VRy)", 6.1), ("D2 (SW)", 5.5)]:
    ax.text(0.15, yy, nm, ha='left', va='center', fontsize=9, family='monospace')
    hline(1.5, 2.4, yy)
    arr((2.2, yy), (2.4, yy))

# ---- T1 Joystick ----
t1 = task(2.4, 4.5, 2.3, 2.6, "Joystick", "20 ms")

# ---- shared vars written by T1 ----
pAIM = pill(5.9, 6.9, "aimX, aimY")
pFIRE = pill(5.9, 5.8, "firePressed")
pCLICK = pill(5.9, 4.8, "clickDone")
for p in (pAIM, pFIRE, pCLICK):
    arr((4.7, p[1]), (p[0], p[1]))        # T1 -> pill

# ---- T2 Game ----
t2 = task(7.6, 4.2, 2.3, 2.6, "Game", "20 ms")
for p in (pAIM, pFIRE, pCLICK):
    arr((p[2], p[1]), (7.6, p[1]))        # pill -> T2

# ---- shared vars written by T2 ----
pSTATE = pill(10.9, 7.7, "gameState")
pTGT = pill(10.9, 6.7, "targetX, targetY")
pSCORE = pill(10.9, 4.6, "score, lives, highScore")
pLOCK = pill(10.9, 3.2, "lockedOn")
for p in (pSTATE, pTGT, pSCORE, pLOCK):
    sy = min(max(p[1], 4.5), 6.5)         # start on the Game box's right edge
    arr((9.9, sy), (p[0], p[1]))          # T2 -> pill

# ---- output tasks ----
t3 = task(12.6, 7.0, 1.9, 1.05, "Matrix", "30 ms")
t4 = task(12.6, 5.55, 1.9, 1.05, "Servo", "40 ms")
t5 = task(12.6, 3.85, 1.9, 1.05, "HUD/TFT", "60 ms")
t6 = task(12.6, 2.25, 1.9, 1.05, "LEDs", "150 ms")

# reads: pills -> output tasks
arr((pSTATE[2], pSTATE[1]), (12.6, 7.85))     # gameState -> Matrix
arr((pTGT[2], pTGT[1]), (12.6, 7.35))         # target -> Matrix
arr((pSCORE[2], pSCORE[1]), (12.6, 4.2))      # score,lives,highScore -> HUD
arr((pSTATE[2], pSTATE[1]), (12.6, 4.05))     # gameState -> HUD
arr((pLOCK[2], pLOCK[1]), (12.6, 2.6))        # lockedOn -> LED
arr((pSTATE[2], pSTATE[1]), (12.6, 2.45))     # gameState -> LED

# aim is also read by Matrix and Servo (route over the top)
ax.plot([pAIM[2], 12.2, 12.2], [pAIM[1], pAIM[1], 7.45], color='#1f7a1f', lw=1.2, zorder=1)
arr((12.2, 7.45), (12.6, 7.45), color='#1f7a1f')     # aim -> Matrix
ax.plot([12.2, 12.2], [7.45, 6.05], color='#1f7a1f', lw=1.2, zorder=1)
arr((12.2, 6.05), (12.6, 6.05), color='#1f7a1f')     # aim -> Servo

# ---- output pins on the right ----
for nm, yy, src in [("LED Matrix (D10,D11,D13)", 7.52, t3),
                    ("Servos (A2, A3)", 6.07, t4),
                    ("TFT (D7-D9, D11, D13)", 4.37, t5),
                    ("Red/Green LEDs (D5, D4)", 2.77, t6)]:
    arr((14.5, yy), (15.0, yy))
    ax.text(15.05, yy, nm, ha='left', va='center', fontsize=9, family='monospace')

ax.text(8, 0.7, "Tasks (rectangles) communicate through shared variables (pills). "
        "A 1 ms GCD scheduler ticks each task at its own period; no queues are used.",
        ha='center', fontsize=9, style='italic')

plt.savefig('task_diagram.png', dpi=150, bbox_inches='tight')
print('wrote task_diagram.png')
