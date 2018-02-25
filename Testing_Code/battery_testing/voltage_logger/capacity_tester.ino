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
5.Consider adding a physical switch to initate test.
*/


/*
state machine logic

if(state!=DONE && cell_voltage<=3.00){                                                  //TESTING --> Complete
    analogWrite(9,0);
    state = 3;
    Serial.println("----------------!!!Discharge Complete!!!----------------");
  }else if(state==TESTING && last_cell_voltage - cell_voltage >= MIN_DROP){             //WAITING --> Testing
    start = millis();
    analogWrite(9,255);
    discharging = true;
    voltage_drop = (start_voltage-cell_voltage);
    internal_resistance = voltage_drop/current;
    Serial.println("----------------!!!Discharging!!!----------------");
    Serial.print("Voltage Drop: ");
    Serial.println(voltage_drop);
  }else if(state==WAITING && start_voltage>=MIN_START_V + 0.05){
    analogWrite(9,255); //start discharge
    state = PREPARING;
  }
  
  if(state==WAITING){                                                            //WAITING           
    start_voltage = cell_voltage;
    analogWrite(9,255);
  }
*/

#define V_REF 1.066     //Arduino analog reference reading voltage(hand calibrated)
#define MIN_DROP 0.1    //minimum voltage drop required for the controller to notice that discharge has started
#define R1 149.8        //resistors for voltage divider to read battery voltages
#define R2 47.2
#define RLOAD 0.03857   //load resistance measure by hand
#define TPIN 9          //transistor driving pin
#define VPIN A0         //analog voltage reading pin
#define MIN_START_V 4.1 //minimum voltage needed to test the cell


//State machine states
#define WAITING 0
#define PREPARING 1 
#define TESTING 2
#define DONE 3

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
short state = 0; 

void setup() {
  Serial.begin(9600);
  analogReference(INTERNAL1V1);
  pinMode(TPIN,OUTPUT);
  Serial.println("----------------Starting----------------");
  last_millis = millis();
}

void loop() {
  //Calculate Celll Voltage and Current
  double voltage_reading = analogRead(VPIN) * (V_REF / 1023.0);
  double current_millis = millis();
  cell_voltage = (voltage_reading/R2)*(R1+R2)*(3.342/3.329);
  current = cell_voltage/RLOAD;

  //State Machine
  if (state == waiting){
    analogWrite(9,0);
    if (button && cell_voltage > MIN_START_V + 0.05){
      analogWrite(9,255);
      state = preparing
    }
  }
if preparing and 
  if voltage <= load_start_v
    contactor = low
  if voltage > min_start_v + margin
    contactor = high
    load_start_v - 0.05 
  if voltage == min_start_v + margin
    contactor = low
    delay by 1 minute
    record start voltage
    state = testing
    contactor = high
    wait 1 milisecond
    measure voltage drop
    start timer
if testing
  if voltage > end_testing_V
    contactor  = high
    count mah 
  if voltage <= end_testing_v
    contactor = low
    state = done

  //Delay Serial Prints
  if(last_millis-current_millis>500){
    printReading(state);
  }

  //Update last cell and last time stuff
  last_cell_voltage = cell_voltage;
  last_millis = current_millis;
  //reading speed
  delay(10);
}

void printReading(byte state){
  //0 - waiting to charge //1 - preparing for test //2 - testing //3 - done testing
  
  //state indication
  switch(state){
    case WAITING:
      if(1000*cell_voltage >= MIN_START_V){
        Serial.print("0 - READY ");
      }else {
        Serial.print("0 - STARTING VOLTAGE TOO LOW (<");
        Serial.print(MIN_START_V);
        Serial.print(")");
      }
      Serial.print("0 - READY ");
      Serial.print("Voltage: ");
      Serial.print(1000*cell_voltage);
      Serial.println("mV");
      break;
    case PREPARING:
      Serial.print("1 - PREPARING ");
      break;
    case TESTING:
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
    case DONE:
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

void prepareCell() {
  //check if starting voltage is high enough

  //determine a load voltage to start recording at
  //wait until battery reach that load voltage
  //disconnect it for a minute to cool it(to not throw off temperature readings)
      //also record the new starting voltage(should be very similar between cells hopefully)
  //connect it back and start counting
  //finish as normal
}
