//Robert Kuramshin, Ankit Kuchhangi
//Created Fall 2017, Updated Spring 2018
//Designed for the battery capacity testing rig.

#define V_REF 1.066     //Arduino analog reference reading voltage (hand calibrated)
#define MIN_DROP 0.1    //minimum voltage drop required for the controller to notice that discharge has started
#define R1 149.8        //resistors for voltage divider to read battery voltages
#define R2 47.2
#define RLOAD 0.03857   //load resistance measure by hand
#define TPIN 9          //transistor driving pin
#define VPIN A0         //analog voltage reading pin
#define TEMP_PIN A1     //temperature reading pin
#define MIN_START_V 4.1 //minimum voltage needed to test the cell
//#define THERMOCOUPLE(x) (m*(x) + b) //turns mV into Celcius (spec sheet)

double start_voltage = 0;
double cell_voltage = 0;
double voltage_reading = 0;
double voltage_drop = 0;
double internal_resistance=0;

unsigned long current_millis = 0;
unsigned long last_millis = 0;
unsigned long print_delay = 0;
double period = 0;

double charge = 0;
double current = 0;
bool discharging = false;
short state = 0; //0 : setup, 1 : discharging, 2 : end / halt, -1 : error;

double getBatteryVoltage();

void setup() {
    Serial.begin(9600);   //9600 bits per second
    analogReference(INTERNAL1V1);     //nominal 1.1 V reference.
    pinMode(TPIN,OUTPUT);     //sets transistor pin to output.
    Serial.print("Starting--v15");
    last_millis = millis();

    cell_voltage = getBatteryVoltage();
    Serial.print("Starting Voltage: "); Serial.println(cell_voltage);

}

void loop() {
    last_millis = current_millis;
    current_millis = millis();
    cell_voltage = getBatteryVoltage();
    //temperature = THERMOCOUPLE(analogRead(TEMP_PIN));
    current = cell_voltage/RLOAD;   //current in A
    
    switch (state) {    //0 : setup, 1 : discharging, 2 : end / halt, -1 : error;
        
    case 0:     //setup and discharge to 3.9V under load (~4.1 open circuit)
        
        if (cell_voltage <= 4.1 && discharging == false) {
            state = -1;
            break;
        }

        discharging = true;

        analogWrite(9,255);     //turns on solenoid. drains until cell_voltage is 4.12V
        if (cell_voltage <= 3.9) {   //start measurement.
            Serial.println("Cell at 3.9V under load");    //replace this with a LED or something.
            analogWrite(9,0);       //shuts off solenoid
            delay(60000);   //1 min delay to let battery cool off.
            start_voltage = getBatteryVoltage();     //preload voltage
            analogWrite(9,255);
            current_millis = millis();
            delay(20);
            cell_voltage = getBatteryVoltage();      //postload voltage
            voltage_drop = (start_voltage-cell_voltage);
            current = cell_voltage/RLOAD;
            internal_resistance = voltage_drop/current;
            state = 1;
        }
    break;

    case 1:     //discharging
        period = current_millis - last_millis;   //period time in ms.
        charge += current*1000 * (period/1000/60/60);   //charge in mAh
        if(current_millis - print_delay > 8000 || cell_voltage < 3)     //print once every 8 seconds.
        {
            Serial.print(charge); Serial.print(","); Serial.println(cell_voltage, 3);
            //Serial.print(","); Serial.println(THERMOCOUPLE(analogRead(TEMP_PIN)), 1);
            print_delay = current_millis;
        }
        if (cell_voltage < 3){
            state = 2;
            analogWrite(9, 0);  //turns off solenoid
            delay(60000);
        }
    break;

    case 2:     //end
        Serial.print("Start Voltage: ");        Serial.print(start_voltage, 3);
        Serial.print(" End Voltage: ");         Serial.print(cell_voltage, 3);
        Serial.print(" Internal Resistance: "); Serial.println(internal_resistance, 6);
        delay(60000);
    break;

    case -1:    //Undercharged battery
        analogWrite(9,0);   //shuts off solenoid
        Serial.println("~AN ERROR HAS OCCURED~");
        Serial.print("Cell Voltage: ");     Serial.println(cell_voltage);
        delay(120000);
    break;
  }
  delay(20);    //delay of 20 ms for accurate voltage measurements.
}

double getBatteryVoltage() {
    voltage_reading = analogRead(VPIN) * (V_REF / 1023.0);
    return ((voltage_reading/R2)*(R1+R2)*(3.342/3.329));
}