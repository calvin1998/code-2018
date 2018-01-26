//Robert Kuramshin
//Created Fall 2017
//Designed for the battery capacity testing rig.

/*
TODO (ordered in no particular order):
1.Currently the starting voltage for each cell is slighly different which makes directly comparing cells hard.
  Consider adding a pre-test procedure which drains the cell to a predetermined voltage from which the cell would be tested. Ex: 4.12V
  This would "even the playing field" but would require cells to be charged past a certain minimum point.
  P.S. This *would not* require tests to be re-run.
2.Clean up outputs, slow down printing speed.
3.Check consistency by retesting a previous cell(not a code thing but idk where else to put it)
4.Based on results from ^ consider recalibrating the results. Could be a good idea to do regularly if its a problem.
*/

#define V_REF 1.066     //Arduino analog reference reading voltage(hand calibrated)
#define MIN_DROP 0.1    //minimum voltage drop required for the controller to notice that discharge has started
#define R1 149.8        //resistors for voltage divider to read battery voltages
#define R2 47.2
#define RLOAD 0.03857   //load resistance measure by hand
#define TPIN 9          //transistor driving pin
#define VPIN A0         //analog voltage reading pin

double start_voltage = 0;
double cell_voltage = 0;
double last_cell_voltage = 0;
double voltage_drop = 0;
double internal_resistance=0;

unsigned long start = millis();
unsigned long last_millis = 0;

double charge;
double current;
bool discharging = false;
bool end_discharge = false;
short state = 0; //0 - waiting to charge //1 - preparing for test //2 - testing //3 - done testing

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
  cell_voltage = (voltage_reading/R2)*(R1+R2)*(3.342/3.329);

  current = cell_voltage/RLOAD;

  if(cell_voltage<=3.00&&state!=3){
    analogWrite(9,0);
    state = 3;
    Serial.println("----------------!!!Discharge Complete!!!----------------");
  }else if(last_cell_voltage - cell_voltage >= MIN_DROP && (state==0 || state==1)){
    start = millis();
    analogWrite(9,255);
    discharging = true;
    voltage_drop = (start_voltage-cell_voltage);
    internal_resistance = voltage_drop/current;
    Serial.println("----------------!!!Discharging!!!----------------");
    Serial.print("Voltage Drop: ");
    Serial.println(voltage_drop);
  }else if(state==0){
    start_voltage = cell_voltage;
    analogWrite(9,255);
  }


  if(last_millis-current_millis>500){
    printReading(state);
  }

  last_cell_voltage = cell_voltage;
  last_millis = current_millis;
  delay(10);
}

void printReading(byte state){
  //0 - waiting to charge //1 - preparing for test //2 - testing //3 - done testing
  
  //state indication
  switch(state){
    case 0:
    Serial.print("0 - READY ");
    Serial.print("Voltage: ");
    Serial.print(1000*cell_voltage);
    Serial.println("mV");
    break;
    case 1:
    Serial.print("1 - PREPARING ");
    break;
    case 2:
    Serial.print("2 - DISCHARGING ");
    double period = current_millis - last_millis;
    charge += current*1000 * (period/1000/60/60);    
    Serial.print("Elapsed Time: ");
    Serial.print((current_millis-start)/1000);
    Serial.print("s Voltage: ");
    Serial.print(1000*cell_voltage);
    Serial.print("mV Current: ");
    Serial.print(1000*current);
    Serial.print("mA Elapsed Charge: ");
    Serial.print(charge);
    Serial.println("mAh");
    break;
    case 3:
     Serial.print("3 - DONE ");
    current = 0;
    Serial.print("Elapsed Time: ");
    Serial.print((current_millis-start)/1000);
    
    Serial.print("s Ellapsed Charge: ");
    Serial.print(charge);

    Serial.print("mAh Start Voltage: ");
    Serial.print(1000*start_voltage);
    
    Serial.print("mV Voltage: ");
    Serial.print(1000*cell_voltage);
 
    Serial.print("mV Current: ");
    Serial.print(1000*current);
    
    Serial.print("mA Voltage Drop: ");
    Serial.print(1000*voltage_drop);
    
    Serial.print("mV Internal Resistance: ");
    Serial.print(1000000*internal_resistance);
    Serial.println("uohm");
    break;
  }
}
