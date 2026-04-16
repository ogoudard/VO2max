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
    lineSplitted = line.split("#")
    
    if len(lineSplitted) == 2:
        data = lineSplitted[1]
        dataSplitted = data.split(",")
        id = int(dataSplitted[0])
        value = float(dataSplitted[1])
        timestamp = int(dataSplitted[2])
        
        if animate.firstMeasure == True:
            animate.firstTimestamp = timestamp
            animate.firstMeasure = False
        
        if id == 1:
            animate.xPressure.append(timestamp)
            animate.pressureArray.append(value)
            #print("Mass flow = " + str(value), flush = True)

            # Draw x and y lists
            ax1.clear()
            ax1.plot(animate.xPressure, animate.pressureArray, color='red')
            ax1.set_ylabel("Mass Flow (l/min)")
            
        elif id == 2:
            animate.xO2.append(timestamp)
            animate.o2Array.append(value)
            #print("O2 = " + str(value), flush = True)

            # Draw x and y lists
            ax2.clear()
            ax2.plot(animate.xO2, animate.o2Array, color='green')
            ax2.set_ylabel("O2 Concentration (%)")
            
        elif id == 3:
            animate.xCo2.append(timestamp)
            animate.co2Array.append(value)
            #print("CO2 = " + str(value), flush = True)

            # Draw x and y lists
            ax3.clear()
            ax3.plot(animate.xO2, animate.co2Array, color='blue')
            #ax3.set_ylabel("CO2 Concentration (ppm)")
            
        elif id == 4:
            animate.xExhaledCycle.append(timestamp)
            animate.exhaledCycleArray.append(value)
            #print("Exhaled volume cycle = " + str(value), flush = True)

            # Draw x and y lists
            ax4.clear()
            ax4.plot(animate.xExhaledCycle, animate.exhaledCycleArray, color='yellow')
            ax4.set_ylabel("Exhaled volume cycle")
            ax4.autoscale()

        elif id == 5:
            animate.xExhaledTotal.append(timestamp)
            animate.exhaledTotalArray.append(value)
            #print("Exhaled volume total = " + str(value), flush = True)

            # Draw x and y lists
            ax5.clear()
            ax5.plot(animate.xExhaledTotal, animate.exhaledTotalArray, color='yellow')
            ax5.set_ylabel("Exhaled volume total")

            
        ax1.set_xlim(left = animate.firstTimestamp, right = timestamp)
        ax2.set_xlim(left = animate.firstTimestamp, right = timestamp)
        ax3.set_xlim(left = animate.firstTimestamp, right = timestamp)
        ax4.set_xlim(left = animate.firstTimestamp, right = timestamp)
        ax5.set_xlim(left = animate.firstTimestamp, right = timestamp)
    
animate.firstMeasure = True

animate.xPressure = []
animate.xO2 = []
animate.xCo2 = []
animate.xExhaledCycle = []
animate.xExhaledTotal = []

animate.pressureArray = []
animate.o2Array = []
animate.co2Array = []
animate.exhaledCycleArray = []
animate.exhaledTotalArray = []

fig, (ax1, ax2, ax3, ax4, ax5) = plt.subplots(5, 1, layout='constrained')

ax1.set_ylabel("Mass Flow (l/min)")
ax2.set_ylabel("O2 Concentration (%)")
ax3.set_ylabel("CO2 Concentration (ppm)")
ax4.set_ylabel("Exhaled volume cycle")
ax5.set_ylabel("Exhaled volume total")
# ax2 = ax1.twinx()
# ax3 = ax1.twinx()
# ax4 = ax1.twinx()

ani = animation.FuncAnimation(fig, animate, frames=1000, interval=50)
plt.show()

