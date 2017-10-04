import serial
import time

ser = serial.Serial('COM10', 115200, timeout=0)
f = open('output.csv', 'w')

f.write("IC, Cell, Voltage, Discharging, Voltage Difference, Distance to Threshold")
while 1:
    s = ser.readline()
    line = ""
    if (s):
        s=s.strip().split()
        if(len(s)==12):
            for i in range(len(s)):
                if(i%2==1):
                    line = line + str(s[i])
                    line = line +","
            line = line +"\n"
            print line
            f.write(line)
f.close()