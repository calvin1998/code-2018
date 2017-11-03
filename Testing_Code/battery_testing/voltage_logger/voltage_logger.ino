#define V_REF 1.067

double current;
double R1 = 150.3;
double R2 = 47.7;
double resistance = 0.033;

void setup() {
  Serial.begin(9600);
  analogReference(INTERNAL1V1);
  start = millis();
}

void loop() {
  double voltage_reading = analogRead(A0) * (V_REF / 1023.0);
  double cell_voltage = (voltage_reading/R2)*(R1+R2);
  current = cell_voltage/resistance;

  Serial.print(millis());
  Serial.print(" ");
  Serial.print(current);
  Serial.print(" ");
  Serial.println(cell_voltage);
  delay(100);
  

}
