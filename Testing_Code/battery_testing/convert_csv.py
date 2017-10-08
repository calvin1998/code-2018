import serial
import time
f_i = open('output.txt', 'r')
f_o = open('output.csv', 'w')

f_o.write("Time, Current, Voltage(Cell), Voltage(Shunt)\n")
for line in f_i.readlines():
    line = line.strip().split()
    line[0]=int(line[0])/1000
    line[0] = str(line[0])
    s = line[0] + "," + line[1] + "," +line[2] + "," + line[3] + "\n"
    f_o.write(s)
f_o.close()
f_i.close()
