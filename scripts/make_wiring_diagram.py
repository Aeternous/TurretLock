import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

fig, ax = plt.subplots(figsize=(13, 9))
ax.set_xlim(0, 13); ax.set_ylim(0, 9.4); ax.axis('off')

SIG = '#1f77b4'   # signal wire color
PWR = '#cc0000'   # +5V
GNDC = '#000000'  # ground

def chip(x, y, w, h, title, color='#e9eef5'):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=color, edgecolor='#333', lw=1.4))
    ax.text(x + w/2, y + h - 0.12, title, ha='center', va='top',
            fontsize=9.5, fontweight='bold')

def pin(x, y, name, side='right'):
    """short pin stub; returns the wire-attach point"""
    if side == 'right':
        ax.plot([x, x + 0.22], [y, y], color='#333', lw=1)
        ax.text(x - 0.06, y, name, ha='right', va='center', fontsize=7)
        return (x + 0.22, y)
    else:  # left
        ax.plot([x - 0.22, x], [y, y], color='#333', lw=1)
        ax.text(x + 0.06, y, name, ha='left', va='center', fontsize=7)
        return (x - 0.22, y)

def wire(p1, p2, color=SIG, channel=None):
    """orthogonal wire: horizontal to channel x, vertical, horizontal to target"""
    cx = channel if channel is not None else (p1[0] + p2[0]) / 2
    ax.plot([p1[0], cx, cx, p2[0]], [p1[1], p1[1], p2[1], p2[1]],
            color=color, lw=1.1, solid_joinstyle='miter')

def wireP(points, color=SIG):
    """orthogonal wire through explicit waypoints"""
    ax.plot([p[0] for p in points], [p[1] for p in points],
            color=color, lw=1.1, solid_joinstyle='miter')

def dot(p):
    ax.plot(p[0], p[1], 'o', color=SIG, ms=3.5)

def gnd(x, y):
    ax.plot([x, x], [y, y - 0.18], color=GNDC, lw=1)
    for i, wdt in enumerate([0.22, 0.14, 0.07]):
        yy = y - 0.18 - i * 0.07
        ax.plot([x - wdt, x + wdt], [yy, yy], color=GNDC, lw=1)

def vcc(x, y):
    ax.plot([x, x], [y, y + 0.18], color=PWR, lw=1)
    ax.text(x, y + 0.26, '+5V', color=PWR, ha='center', va='bottom', fontsize=6.5)

def resistor(x, y):
    ax.add_patch(Rectangle((x - 0.18, y - 0.06), 0.36, 0.12,
                 facecolor='white', edgecolor='#333', lw=1))
    ax.text(x, y + 0.13, '220', ha='center', va='bottom', fontsize=6)

# ---------------- Arduino with right-side pins ----------------
chip(0.7, 1.4, 2.0, 6.4, 'Arduino\nUno R3', '#dff0d8')
order = ['D13','D11','D10','D9','D8','D7','A0','A1','D2','A2','A3','D5','D4']
P = {}
for i, nm in enumerate(order):
    yy = 7.3 - i * 0.46
    P[nm] = pin(2.7, yy, nm, 'right')

# ---------------- peripherals (left-side terminals) ----------------
chip(5.6, 6.5, 2.6, 1.3, 'MAX7219 Matrix')
mDIN = pin(5.6, 7.5, 'DIN', 'left'); mCLK = pin(5.6, 7.1, 'CLK', 'left'); mCS = pin(5.6, 6.7, 'CS', 'left')
vcc(6.9, 7.8); gnd(7.6, 6.5)

chip(5.6, 3.4, 2.6, 2.4, 'ST7735 TFT')
tCS = pin(5.6, 5.5, 'CS', 'left'); tDC = pin(5.6, 5.1, 'DC', 'left'); tRST = pin(5.6, 4.7, 'RST', 'left')
tSDA = pin(5.6, 4.3, 'SDA', 'left'); tSCK = pin(5.6, 3.9, 'SCK', 'left')
vcc(6.5, 5.8); gnd(7.0, 3.4)

chip(10.0, 6.0, 2.4, 1.4, 'Joystick')
jX = pin(10.0, 7.1, 'VRx', 'left'); jY = pin(10.0, 6.7, 'VRy', 'left'); jSW = pin(10.0, 6.3, 'SW', 'left')
vcc(11.2, 7.4); gnd(11.9, 6.0)

chip(9.7, 3.0, 1.5, 1.1, 'Servo X')
sX = pin(9.7, 3.5, 'SIG', 'left')
ax.text(10.45, 3.0, 'V+ ext 5V / GND', ha='center', va='top', fontsize=6)
chip(11.4, 3.0, 1.5, 1.1, 'Servo Y')
sY = pin(11.4, 3.5, 'SIG', 'left')

# LEDs with series resistors to ground
chip(5.6, 1.4, 1.6, 0.9, 'Red LED')
rLED = pin(5.6, 1.85, 'A', 'left'); gnd(7.0, 1.4)
chip(8.0, 1.4, 1.6, 0.9, 'Green LED')
gLED = pin(8.0, 1.85, 'A', 'left'); gnd(9.4, 1.4)

# ---------------- wiring (orthogonal, per-net channel) ----------------
wire(P['D10'], mCS,  channel=4.9)
wire(P['D13'], mCLK, channel=4.6); wire(P['D13'], tSCK, channel=4.6); dot((4.6, P['D13'][1]))
wire(P['D11'], mDIN, channel=4.4); wire(P['D11'], tSDA, channel=4.4); dot((4.4, P['D11'][1]))
wire(P['D9'],  tCS,  channel=5.1)
wire(P['D8'],  tDC,  channel=5.0)
wire(P['D7'],  tRST, channel=4.8)
# joystick: route up through the clear gap between TFT (top 5.8) and matrix (bot 6.5)
wireP([P['A0'], (3.7, P['A0'][1]), (3.7, 6.00), (9.5, 6.00), (9.5, jX[1]), jX])
wireP([P['A1'], (3.85, P['A1'][1]), (3.85, 6.12), (9.65, 6.12), (9.65, jY[1]), jY])
wireP([P['D2'], (4.0, P['D2'][1]), (4.0, 6.24), (9.78, 6.24), (9.78, jSW[1]), jSW])
# servos: route low, below the TFT (bot 3.4) and above the LEDs (top 2.3)
wireP([P['A2'], (3.7, P['A2'][1]), (3.7, 2.62), (9.3, 2.62), (9.3, sX[1]), sX])
wireP([P['A3'], (3.85, P['A3'][1]), (3.85, 2.50), (11.0, 2.50), (11.0, sY[1]), sY])
# LED lines through a resistor
wire(P['D5'], (5.0, rLED[1]), channel=4.5); resistor(5.2, rLED[1]); wire((5.38, rLED[1]), rLED)
wire(P['D4'], (7.5, gLED[1]), channel=4.3); resistor(7.7, gLED[1]); wire((7.88, gLED[1]), gLED)

ax.text(6.5, 9.1, 'TurretLock - Schematic', ha='center', fontsize=13, fontweight='bold')
ax.text(6.5, 0.35,
        'D11 (MOSI) and D13 (SCK) are a shared SPI bus to the matrix and TFT (junction dots). '
        'TFT backlight (LED) ties to +5V through a resistor. '
        'Servos take +5V from an external supply with a common ground.',
        ha='center', fontsize=7, style='italic')

plt.savefig('images/wiring_diagram.png', dpi=150, bbox_inches='tight')
print('wrote wiring_diagram.png')
