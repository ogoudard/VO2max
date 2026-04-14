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

def animate(i, xs, ys):
    line = ser.readline().decode("ascii")
    if line.find("CO2") != -1:
        line = line.split('=')[1]
        co2 = float(re.findall(r"[-+]?(?:\d*\.*\d+)", line)[0])

        # Add x and y to lists
        animate.xCo2.append(int(time.time()) - startTime)
        animate.iCo2 = animate.iCo2 + 1
        animate.co2Array.append(co2)

        # Draw x and y lists
        ax3.clear()
        ax3.plot(animate.xCo2, animate.co2Array, color='red')
        
    elif line.find("O2") != -1:
        line = line.split('=')[1]
        o2 = float(re.findall(r"[-+]?(?:\d*\.*\d+)", line)[0])  
        
        # Add x and y to lists
        animate.xO2.append(int(time.time()) - startTime)
        animate.iO2 = animate.iO2 + 1
        animate.o2Array.append(o2)

        # Draw x and y lists
        ax2.clear()
        ax2.plot(animate.xO2, animate.o2Array, color='green')

    elif line.find("pressure") != -1:
        line = line.split('=')[1]
        pressure = float(re.findall(r"[-+]?(?:\d*\.*\d+)", line)[0])
       
        # Add x and y to lists
        animate.xPressure.append(int(time.time()) - startTime)
        animate.iPressure = animate.iPressure + 1
        animate.pressureArray.append(pressure)

        # Draw x and y lists
        ax1.clear()
        ax1.plot(animate.xPressure, animate.pressureArray, color='blue')

animate.iPressure = 0
animate.iO2 = 0
animate.iCo2 = 0

animate.xPressure = []
animate.xO2 = []
animate.xCo2 = []

animate.pressureArray = []
animate.o2Array = []
animate.co2Array = []




# Create figure for plotting
fig, ax1 = plt.subplots()

ax2 = ax1.twinx()
ax3 = ax1.twinx()

ax1.set_ylabel("Differential Pressure (hPa)")
ax2.set_ylabel("O2")
ax3.set_ylabel("CO2")

# Format plot
plt.title('VO2max')
xs = []
ys = []

fig.tight_layout()
ani = animation.FuncAnimation(fig, animate, fargs=(xs, ys), interval=5)
plt.show()

