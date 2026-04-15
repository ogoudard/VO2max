import serial
import sys
import numpy as np
import re
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import time

ser = serial.Serial(
    port=sys.argv[1],\
    baudrate=115200,\
    parity=serial.PARITY_NONE,\
    stopbits=serial.STOPBITS_ONE,\
    bytesize=serial.EIGHTBITS,\
        timeout=10000)
        
startTime = int(time.time())

def animate(i):
    line = ser.readline().decode("ascii").replace("\r\n", "")
    data = line.split("#")[1]
    dataSplitted = data.split(",")
    id = int(dataSplitted[0])
    value = float(dataSplitted[1])
    timestamp = int(dataSplitted[2])
    
    if id == 1:
        animate.xPressure.append(timestamp)
        animate.pressureArray.append(value)

        # Draw x and y lists
        ax1.clear()
        ax1.plot(animate.xPressure, animate.pressureArray, color='red')
        ax1.set_ylabel("Mass Flow (l/min)")
        
    elif id == 2:
        animate.xCo2.append(timestamp)
        animate.co2Array.append(value)

        # Draw x and y lists
        ax2.clear()
        ax2.plot(animate.xCo2, animate.co2Array, color='green')
        ax2.set_ylabel("O2 Concentration (%)")

    elif id == 3:
        animate.xO2.append(timestamp)
        animate.o2Array.append(value)

        # Draw x and y lists
        ax3.clear()
        ax3.plot(animate.xO2, animate.o2Array, color='blue')
        ax3.spines.right.set_position(("axes", 1.2))
        ax3.set_ylabel("CO2 Concentration (ppm)")

    elif id == 4:
        animate.xExhaled.append(timestamp)
        animate.exhaledArray.append(value)

        # Draw x and y lists
        ax4.clear()
        ax4.plot(animate.xExhaled, animate.exhaledArray, color='yellow')
        ax4.spines.left.set_position(("axes", 1.2))
        ax4.set_ylabel("Exhaled volume")
        
animate.xPressure = []
animate.xO2 = []
animate.xCo2 = []
animate.xExhaled = []

animate.pressureArray = []
animate.o2Array = []
animate.co2Array = []
animate.exhaledArray = []

fig, ax1 = plt.subplots()
fig.subplots_adjust(right=0.75)

ax2 = ax1.twinx()
ax3 = ax1.twinx()
ax4 = ax1.twinx()

# Offset the right spine of twin2.  The ticks and label have already been
# placed on the right by twinx above.
ax3.spines.right.set_position(("axes", 1.2))
ax4.spines.left.set_position(("axes", 1.2))

p1, = ax1.plot([0, 1, 2], [0, 1, 2], "r-")
p2, = ax2.plot([0, 1, 2], [0, 3, 2], "g-")
p3, = ax3.plot([0, 1, 2], [50, 30, 15], "b-")
p4, = ax4.plot([0, 1, 2], [50, 30, 15], "y-")

ax1.set_xlabel("Time")
ax1.set_ylabel("Mass Flow")
ax2.set_ylabel("O2 Concentration")
ax3.set_ylabel("CO2 Concentration")
ax4.set_ylabel("Exhaled volume")


ax1.yaxis.label.set_color(p1.get_color())
ax2.yaxis.label.set_color(p2.get_color())
ax3.yaxis.label.set_color(p3.get_color())
ax4.yaxis.label.set_color(p4.get_color())

tkw = dict(size=4, width=1.5)
ax1.tick_params(axis='y', colors=p1.get_color(), **tkw)
ax2.tick_params(axis='y', colors=p2.get_color(), **tkw)
ax3.tick_params(axis='y', colors=p3.get_color(), **tkw)
ax4.tick_params(axis='y', colors=p4.get_color(), **tkw)

ax1.tick_params(axis='x', **tkw)

ax1.legend(handles=[p1, p2, p3, p4])

ani = animation.FuncAnimation(fig, animate, frames=1000, interval=50)
plt.show()

