import serial
import sys
import numpy as np
import matplotlib.pyplot as plt

plt.axis([0, 10, 0, 1])


ser = serial.Serial(
    port=sys.argv[1],\
    baudrate=115200,\
    parity=serial.PARITY_NONE,\
    stopbits=serial.STOPBITS_ONE,\
    bytesize=serial.EIGHTBITS,\
        timeout=0)
        
while(1):
    print(ser.readline().decode("ascii"))
    
    #plt.scatter(i, y)
    #plt.pause(0.05)

    #plt.show()