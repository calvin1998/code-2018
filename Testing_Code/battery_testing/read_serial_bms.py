import serial
import time
import os
clear = lambda: os.system('cls')

ser = serial.Serial('COM10', 115200, timeout=0)
f = open('output.csv', 'w')
min_cell = 999.0
f.write("IC, Cell, Voltage, Discharging, Voltage Difference, Distance to Threshold")
while 1:
    s = ser.readline()
    line = ""
    if (s):
        #print s
        s=s.strip().split()
        if (float(s[5])<min_cell):
            min_cell = float(s[5])
            print min_cell
        if(len(s)==12):
            for i in range(len(s)):
                if(i%2==1):
                    line = line + str(s[i])
                    line = line +","
            line = line +"\n"
            f.write(line)
f.close()