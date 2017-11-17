#define V_REF 1.066
#define MIN_DROP 0.1
#define R1 149.8
#define R2 47.2
#define RLOAD 0.03857
#define TPIN 9
#define VPIN A0

double cell_voltage = 0;
double last_cell_voltage = 0;

unsigned long start = millis();
unsigned long last_millis = 0;

double charge;
double current;
bool discharging = false;
bool end_discharge = false;

void setup() {
  Serial.begin(9600);
  analogReference(INTERNAL1V1);
  pinMode(TPIN,OUTPUT);
  Serial.println("----------------Starting----------------");
  last_millis = millis();
}

void loop() {
  double voltage_reading = analogRead(VPIN) * (V_REF / 1023.0);
  double current_millis = millis();
  cell_voltage = (voltage_reading/R2)*(R1+R2)*(3.724/3.71);

  current = cell_voltage/RLOAD;

  if(cell_voltage<=3.00&&!end_discharge){
    analogWrite(9,0);
    end_discharge = true;
    discharging = false;
    Serial.println("----------------!!!Discharge Complete!!!----------------");
  }else if(last_cell_voltage - cell_voltage >= MIN_DROP && !discharging&&!end_discharge){
    start = millis();
    analogWrite(9,255);
    discharging = true;
    Serial.println("----------------!!!Discharging!!!----------------");
  }

  if(discharging){
   
    double period = current_millis - last_millis;
    charge += current*1000 * (period/1000/60/60);    
    Serial.print("Elapsed Time: ");
    Serial.print((current_millis-start)/1000);
    Serial.print("s Voltage: ");
    Serial.print(cell_voltage);
    Serial.print("V Current: ");
    Serial.print(current);
    Serial.print("A Elapsed Charge: ");
    Serial.print(charge);
    Serial.println("mA");
  }else if(end_discharge){
    current = 0;
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
