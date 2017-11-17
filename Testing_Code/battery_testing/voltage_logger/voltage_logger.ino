#define V_REF 1.066
#define MIN_DROP 0.1

double current;
double R1 = 149.8;
double R2 = 47.2;
double resistance = 0.03857;

double cell_voltage = 0;
double last_cell_voltage = 0;

unsigned long start = millis();
unsigned long last_millis = 0;

double charge;
bool discharging = false;
bool master_trip = false;

void setup() {
  Serial.begin(9600);
  analogReference(INTERNAL1V1);
  pinMode(9,OUTPUT);
  Serial.println("----------------Starting----------------");
  last_millis = millis();
}

void loop() {
  double voltage_reading = analogRead(A0) * (V_REF / 1023.0);
  double current_millis = millis();
  cell_voltage = (voltage_reading/R2)*(R1+R2)*(3.724/3.71);

  if(cell_voltage<=3.00&&!master_trip){
    analogWrite(9,0);
    master_trip = true;
    discharging = false;
    Serial.println("----------------!!!Discharge Complete!!!----------------");
  }else if(!master_trip){
    analogWrite(9,255);
  }
  current = cell_voltage/resistance;

  if(last_cell_voltage - cell_voltage >= MIN_DROP && !discharging&&!master_trip){
    start = millis();
    //discharge start
    discharging = true;
    Serial.println("----------------!!!Discharging!!!----------------");
  }
  if(discharging || master_trip){
    if(!master_trip){
      double period = current_millis - last_millis;
      charge += current*1000 * (period/1000/60/60);
    }else{
      current =0;
    }
    Serial.print("Elapsed Time: ");
    Serial.print((current_millis-start)/1000);
    Serial.print("s Voltage: ");
    Serial.print(cell_voltage);
    Serial.print("V Current: ");
    Serial.print(current);
    Serial.print("A Elapsed Charge: ");
    Serial.print(charge);
    Serial.println("mA");
  }else{
    Serial.print("Voltage: ");
    Serial.println(cell_voltage);
  }



  last_cell_voltage = cell_voltage;
  last_millis = current_millis;
  delay(100);
  

}
