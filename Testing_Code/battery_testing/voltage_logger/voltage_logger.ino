double r = 0.06/250;
double current;
double R1 = 2183;
double R2 = 330.5;

void setup() {
  Serial.begin(9600);
  //analogReference(EXTERNAL);
}

void loop() {
  int currentReading = analogRead(A14);
  double ref = analogRead(A7) * (4.455 / 1023.0);
  double voltage_drop = currentReading * (4.455 / 1023.0)/(ref/3.3);
  int voltageReading = analogRead(A0);
  double cell_voltage = voltageReading * (4.455 / 1023.0)/(ref/3.3);
  //cell_voltage = (cell_voltage*(2524.0))/331.2;
  current = (voltage_drop/r)/35;

  Serial.print(millis());
  Serial.print(" ");
  Serial.print(current);
  Serial.print(" ");
  Serial.print(cell_voltage);
  Serial.print(" ");
  Serial.println(voltage_drop);
  delay(100);
  

}
