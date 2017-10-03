import serial
import time
ser = serial.Serial('COM17', 9600, timeout=0)
f = open('output.txt', 'w')

while 1:
 try:
  s = ser.readline()
  if (s):
    print (s)
    f.write(s)
 except ser.SerialTimeoutException:
  print('Data could not be read')
  time.sleep(1)
