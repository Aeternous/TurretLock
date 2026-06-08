import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

fig, ax = plt.subplots(figsize=(15, 9))
ax.set_xlim(0, 15); ax.set_ylim(0, 9.6); ax.axis('off')

boxes = {}
def box(name, cx, cy, w, h, label, color, style="round"):
    boxes[name] = (cx, cy, w, h)
    bs = "round,pad=0.02,rounding_size=0.12" if style == "round" else "square,pad=0.02"
    ax.add_patch(FancyBboxPatch((cx-w/2, cy-h/2), w, h, boxstyle=bs,
                 facecolor=color, edgecolor='#333', lw=1.3, zorder=3))
    ax.text(cx, cy, label, ha='center', va='center', fontsize=8, zorder=4)

def anchor(b, tx, ty):
    cx, cy, w, h = b
    dx, dy = tx-cx, ty-cy
    if dx == 0 and dy == 0:
        return (cx, cy)
    sx = (w/2)/abs(dx) if dx else 1e9
    sy = (h/2)/abs(dy) if dy else 1e9
    s = min(sx, sy)
    return (cx+dx*s, cy+dy*s)

def arrow(a, b, color='#444', waypoints=None):
    ba, bb = boxes[a], boxes[b]
    if waypoints:
        p1 = anchor(ba, waypoints[0][0], waypoints[0][1])
        pts = [p1] + waypoints + [anchor(bb, waypoints[-1][0], waypoints[-1][1])]
        ax.plot([p[0] for p in pts[:-1]], [p[1] for p in pts[:-1]],
                color=color, lw=1.0, zorder=1)
        ax.add_patch(FancyArrowPatch(pts[-2], pts[-1], arrowstyle='-|>',
                     mutation_scale=11, color=color, lw=1.0, zorder=1))
    else:
        p1 = anchor(ba, bb[0], bb[1]); p2 = anchor(bb, ba[0], ba[1])
        ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle='-|>',
                     mutation_scale=11, color=color, lw=1.0, zorder=1))

GREEN='#d5e8d4'; BLUE='#dae8fc'; YELL='#fff2cc'; ORAN='#ffe6cc'

# inputs
box('ADC', 1.2, 7.0, 2.0, 0.95, "Joystick X/Y\nADC A0, A1", GREEN)
box('BTN', 1.2, 4.5, 2.0, 0.95, "Button\nD2 (pull-up)", GREEN)
# tasks (name + period)
box('T1', 3.8, 5.75, 1.8, 1.1, "T1 Joystick\n20 ms", BLUE)
box('T2', 6.7, 5.75, 1.9, 1.1, "T2 Game SM\n20 ms", BLUE)
box('T3', 11.1, 8.2, 1.7, 0.95, "T3 Matrix\n30 ms", BLUE)
box('T4', 11.1, 6.4, 1.7, 0.95, "T4 Servo\n40 ms", BLUE)
box('T5', 11.1, 4.2, 1.7, 0.95, "T5 HUD/TFT\n60 ms", BLUE)
box('T6', 11.1, 2.2, 1.7, 0.95, "T6 LEDs\n150 ms", BLUE)
# shared variables (name : type)
box('VAIM', 5.2, 7.1, 1.6, 0.8, "aimX, aimY\n: uchar", YELL, "square")
box('VBTN', 5.2, 4.4, 1.7, 0.8, "firePressed,\nclickDone : uchar", YELL, "square")
box('VSTATE', 8.8, 8.5, 1.7, 0.7, "gameState : uchar", YELL, "square")
box('VTGT', 8.8, 7.2, 1.7, 0.7, "targetX,Y : uchar", YELL, "square")
box('VSCORE', 8.8, 4.5, 1.8, 0.85, "score:uint, lives:uchar,\nhighScore:uint", YELL, "square")
box('VLOCK', 8.8, 2.9, 1.7, 0.7, "lockedOn : uchar", YELL, "square")
# outputs
box('OMAT', 13.6, 8.2, 1.7, 0.95, "LED Matrix\n(MAX7219)", ORAN)
box('OSRV', 13.6, 6.4, 1.7, 0.95, "Servos\n(OCR1A/B)", ORAN)
box('OTFT', 13.6, 4.2, 1.7, 0.95, "TFT screen", ORAN)
box('OLED', 13.6, 2.2, 1.7, 0.95, "Red/Green\nLEDs", ORAN)

# edges: inputs -> T1
arrow('ADC', 'T1'); arrow('BTN', 'T1')
# T1 writes shared vars
arrow('T1', 'VAIM'); arrow('T1', 'VBTN')
# reads of aim/button by Game
arrow('VAIM', 'T2'); arrow('VBTN', 'T2')
# aim also read by Matrix and Servo (routed cleanly over the top)
arrow('VAIM', 'T4', color='#2e7d32', waypoints=[(5.2, 9.0), (10.0, 9.0), (10.0, 6.4)])
arrow('VAIM', 'T3', color='#2e7d32', waypoints=[(5.2, 9.25), (10.15, 9.25), (10.15, 8.2)])
# T2 writes shared vars
arrow('T2', 'VSTATE'); arrow('T2', 'VTGT'); arrow('T2', 'VSCORE'); arrow('T2', 'VLOCK')
# reads by output tasks
arrow('VSTATE', 'T3'); arrow('VSTATE', 'T5'); arrow('VSTATE', 'T6')
arrow('VTGT', 'T3'); arrow('VSCORE', 'T5'); arrow('VLOCK', 'T6')
# tasks -> hardware outputs
arrow('T3', 'OMAT'); arrow('T4', 'OSRV'); arrow('T5', 'OTFT'); arrow('T6', 'OLED')

ax.text(7.5, 9.5, "TurretLock - Task Diagram", ha='center', fontsize=14, fontweight='bold')
ax.text(7.5, 9.1, "scheduled by a 1 ms GCD tick (Timer2); no queues are used",
        ha='center', fontsize=8.5, style='italic')
# legend
for i, (c, t) in enumerate([(GREEN,'input'), (BLUE,'task (period)'),
                            (YELL,'shared variable (type)'), (ORAN,'hardware output')]):
    ax.add_patch(FancyBboxPatch((1.0 + i*3.3, 0.45), 0.35, 0.3, boxstyle="square,pad=0.02",
                 facecolor=c, edgecolor='#333', lw=1))
    ax.text(1.45 + i*3.3, 0.6, t, ha='left', va='center', fontsize=8.5)

plt.savefig('task_diagram.png', dpi=150, bbox_inches='tight')
print('wrote task_diagram.png')
