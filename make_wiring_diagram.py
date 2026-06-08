import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

fig, ax = plt.subplots(figsize=(9, 6.5))
ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.axis('off')

def box(x, y, w, h, title, lines, color):
    ax.add_patch(FancyBboxPatch((x, y), w, h,
        boxstyle="round,pad=0.02,rounding_size=0.10",
        linewidth=1.3, edgecolor='#333333', facecolor=color))
    ax.text(x + w/2, y + h - 0.26, title, ha='center', va='top',
            fontsize=10, fontweight='bold')
    ax.text(x + w/2, y + h - 0.62, "\n".join(lines), ha='center', va='top', fontsize=8)

def line(x1, y1, x2, y2):
    ax.plot([x1, x2], [y1, y2], color='#666666', lw=1.0, zorder=0)

# central controller
box(3.7, 4.2, 2.7, 1.6, "Arduino Uno R3", ["ATmega328P", "5V / GND rails"], '#dff0d8')

# peripherals
box(3.3, 8.2, 3.4, 1.4, "LED Matrix (MAX7219)", ["CS=D10  DIN=D11", "CLK=D13   5V  GND"], '#fde9d9')
box(0.2, 6.3, 2.9, 2.0, "TFT (ST7735)", ["CS=D9  DC=D8  RST=D7", "SDA=D11  SCK=D13", "5V  GND  LED->5V"], '#dbe5f1')
box(7.2, 6.6, 2.6, 1.7, "Joystick", ["VRx=A0  VRy=A1", "SW=D2   5V  GND"], '#e5dfec')
box(0.3, 2.1, 2.1, 1.2, "Red LED", ["D5 -> R -> GND"], '#f2dcdb')
box(0.3, 0.5, 2.1, 1.2, "Green LED", ["D4 -> R -> GND"], '#dff0d8')
box(3.6, 0.5, 2.8, 1.5, "Servo X / Servo Y", ["sig = A2 / A3", "V+ = 5V (ext)  GND"], '#fff2cc')

# connections (Uno edge -> component)
line(5.05, 5.8, 5.0, 8.2)    # matrix
line(3.7, 5.4, 3.1, 7.2)     # tft
line(6.4, 5.4, 7.2, 7.3)     # joystick
line(3.7, 4.7, 2.4, 2.7)     # red led
line(3.75, 4.4, 2.4, 1.3)    # green led
line(5.05, 4.2, 5.0, 2.0)    # servos

ax.text(5, 9.85, "TurretLock - Wiring Diagram", ha='center', fontsize=13, fontweight='bold')
ax.text(5, 0.06,
        "Shared SPI: D11 = MOSI (matrix DIN + TFT SDA), D13 = SCK. "
        "Servos powered from external 5V with a common ground.",
        ha='center', fontsize=7.5, style='italic')

plt.savefig('wiring_diagram.png', dpi=150, bbox_inches='tight')
print('wrote wiring_diagram.png')
