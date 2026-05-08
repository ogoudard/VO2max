import serial
import sys


ser = serial.Serial(
    port=sys.argv[1],\
    baudrate=921600,\
    parity=serial.PARITY_NONE,\
    stopbits=serial.STOPBITS_ONE,\
    bytesize=serial.EIGHTBITS,\
        timeout=10000)
       
TEMPERATURE_LOG_ID=0
HUMIDITY_LOG_ID=1
PRESSURE_LOG_ID=2
ALTITUDE_LOG_ID=3
FLOW_LOG_ID=4
CYCLE_EXHALED_VOLUME_LOG_ID=5
TOTAL_EXHALED_VOLUME_LOG_ID=6
O2_LOG_ID=7
CO2_LOG_ID=8
VO2_LOG_ID=9
VO2MAX_LOG_ID=10
VCO2_LOG_ID=11
RQ_LOG_ID=12
RR_LOG_ID=13
RHO_LOG_ID=14
EXPIRATORY_FLOW_ID=15

flow = 0
temperature = 0
humidity = 0pressure = 0
altitude = 0
cycleVolume = 0
totalVolume = 0
o2 = 0
co2 = 0
vO2 = 0
vO2max = 0
vCo2 = 0
rq = 0
rr = 0
rho = 0
expiratoryFlow = 0
                
if __name__ == "__main__":
    with open(sys.argv[2], 'w+') as file:
        file.write("timestamp,temperature,humidity,pressure,altitude,flow,expiratory_flow,cycle_volume,total_volume,o2,co2,vo2,vo2max,vco2,rq,rr,rho\n")
        
        while(1):
            line = ser.readline().decode('ascii')
            splittedLine = line.split(',')
            id = int(splittedLine[0])
            data = float(splittedLine[1])
            timestamp = int(splittedLine[2])
            
            if id == FLOW_LOG_ID:
                flow = data
            elif id == TEMPERATURE_LOG_ID:
                temperature = data
            elif id == HUMIDITY_LOG_ID:
                humidity = data
            elif id == PRESSURE_LOG_ID:
                pressure = data
            elif id == ALTITUDE_LOG_ID:
                altitude = data
            elif id == CYCLE_EXHALED_VOLUME_LOG_ID:
                cycleVolume = data
            elif id == TOTAL_EXHALED_VOLUME_LOG_ID:
                totalVolume = data
            elif id == O2_LOG_ID:
                o2 = data
            elif id == CO2_LOG_ID:
                co2 = data
            elif id == VO2_LOG_ID:
                vO2 = data
            elif id == VO2MAX_LOG_ID:
                vO2max = data
            elif id == VCO2_LOG_ID:
                vCo2 = data
            elif id == RQ_LOG_ID:
                rq = data
            elif id == RR_LOG_ID:
                rr = data
            elif id == RHO_LOG_ID:
                rho = data
            elif id == EXPIRATORY_FLOW_ID:
                expiratoryFlow = data
                
            fileLine = "{:d},{:.0f},{:.1f},{:.0f},{:.0f},{:.1f},{:.2f},{:.2f},{:.2f},{:.1f},{:.0f},{:.1f},{:.1f},{:.1f},{:.2f},{:.1f},{:.1f}\n".format(timestamp, temperature, humidity, pressure, altitude, flow, expiratoryFlow, cycleVolume, totalVolume, o2, co2, vO2, vO2max, vCo2, rq, rr, rho)
            file.write(fileLine)