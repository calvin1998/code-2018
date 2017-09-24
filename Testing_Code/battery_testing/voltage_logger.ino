  /**
 * LinduinoBMS.ino - Code that runs on the BMS Linduino. This system only has one Linduino, which interfaces directly with the Car's CAN Bus
 * Created by Shrivathsav Seshan & Charith "Karvin" Dassanayake, April 11, 2017
 */

#include <Arduino.h>
#include "mcp_can.h"
#include "LTC68041.h"
#include "HyTech17.h"

/*
 * 
 * On startup.
 * 1. GLV boxes latches shutdown circuit closed.
 * 2. AIR's close.
 * 3. Voltage flows out of box, and turn on lights.
 * 4. Any faults (OK_HS, BMS_OK, BSPD) will open shutdown circuit, open AIR's.
 */

/*
 * Operating condition notes:
 * 1. BMS sensors can be powered at all times.
 * 2. Once linduino gets power from external power lines, give OK_BMS signal.
 * 3. No need to check DC bus voltage, because all batteries read their true voltages at all times. (They are continuous with each other at all times, due to no relay.)
 * 4. Once Temps go too high, current goes too high, or cell voltages go too high or too low, drive the BMS_OK signal low.
 */

/************BATTERY CONSTRAINTS AND CONSTANTS**********************/
short voltage_cutoff_low = 2980;
short voltage_cutoff_high = 4210;
short total_voltage_cutoff = 150;
short discharge_current_constant_high = 220;
short charge_current_constant_high = -400;
short max_val_current_sense = 300;
short charge_temp_critical_high = 4400;// 44.00
short discharge_temp_critical_high = 6000; // 60.00
short voltage_difference_threshold = 1000; //100 mV, 0.1V

#define ENABLE_CAN false // use this definition to enable or disable CAN
/********GLOBAL ARRAYS/VARIABLES CONTAINING DATA FROM CHIP**********/
#define TOTAL_IC 1 // DEBUG: We have temporarily overwritten this value
#define TOTAL_CELLS 9
#define TOTAL_THERMISTORS 3 // TODO: Double check how many thermistors are being used.
#define THERMISTOR_RESISTOR_VALUE 6700 // TODO: Double check what resistor is used on the resistor divider.
uint16_t cell_voltages[TOTAL_IC][12]; // contains 12 battery cell voltages. Numbers are stored in 0.1 mV units.
uint16_t aux_voltages[TOTAL_IC][6]; // contains auxiliary pin voltages.
     /* Data contained in this array is in this format:
      * Thermistor 1
      * Thermistor 2
      * Thermistor 3
      */
int16_t cell_delta_voltage[TOTAL_IC][9]; // keep track of which cells are being discharged
int16_t ignore_cell[TOTAL_IC][9]; //cells to be ignored for Balance testing

/*!<
  The tx_cfg[][6] sto the LTC6804 configuration data that is going to be written
  to the LTC6804 ICs on the daisy chain. The LTC6804 configuration data that will be
  written should be stored in blocks of 6 bytes. The array should have the following format:

 |  tx_cfg[0][0]| tx_cfg[0][1] |  tx_cfg[0][2]|  tx_cfg[0][3]|  tx_cfg[0][4]|  tx_cfg[0][5]| tx_cfg[1][0] |  tx_cfg[1][1]|  tx_cfg[1][2]|  .....    |
 |--------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|-----------|
 |IC1 CFGR0     |IC1 CFGR1     |IC1 CFGR2     |IC1 CFGR3     |IC1 CFGR4     |IC1 CFGR5     |IC2 CFGR0     |IC2 CFGR1     | IC2 CFGR2    |  .....    |

*/
uint8_t tx_cfg[TOTAL_IC][6]; // data defining how data will be written to daisy chain ICs.

/**
 * CAN Variables
 */
#define CAN_SPI_CS_PIN 10
MCP_CAN CAN(CAN_SPI_CS_PIN);
long msTimer = 0;

/**
 * BMS State Variables
 */
#define BMS_OK_PIN 3
#define WATCH_DOG_TIMER 4
#define CURRENT_SENSE 2
bool watchDogFlag = true;
BMS_voltages bmsVoltageMessage;
BMS_currents bmsCurrentMessage;
BMS_temperatures bmsTempMessage;
BMS_status bmsStatusMessage;

int minVoltageICIndex;
int minVoltageCellIndex;
int voltage_difference;

bool cell_discharging[TOTAL_IC][12];

void setup() {
    // put your setup code here, to run once:
    pinMode(BMS_OK_PIN, OUTPUT);
    pinMode(WATCH_DOG_TIMER, OUTPUT);
    pinMode(CAN_SPI_CS_PIN, OUTPUT);// Not needed, done in mcp_can.cpp

    digitalWrite(CAN_SPI_CS_PIN, HIGH);
    digitalWrite(BMS_OK_PIN, HIGH);

    Serial.begin(115200);
    delay(2000);

    LTC6804_initialize();
    init_cfg();
    poll_cell_voltage();
    memcpy(cell_delta_voltage, cell_voltages, 2 * TOTAL_IC * TOTAL_CELLS);
    bmsCurrentMessage.setChargingState(CHARGING);
    Serial.println("Setup Complete!");
    
}

// NOTE: Implement Coulomb counting to track state of charge of battery.
/*
 * Main BMS Control Loop
 */

void loop() {
    process_voltages(); // polls controller, and sto data in bmsVoltageMessage object.
    process_temps(); // sto datap in bmsTempMessage object.

}

/*!***********************************
 \brief Initializes the configuration array
 **************************************/
void init_cfg()
{
    for(int i = 0; i < TOTAL_IC; i++)
    {
        tx_cfg[i][0] = 0xFE;
        tx_cfg[i][1] = 0x00 ;
        tx_cfg[i][2] = 0x00 ;
        tx_cfg[i][3] = 0x00 ;
        tx_cfg[i][4] = 0x00 ;
        tx_cfg[i][5] = 0x00 ;
    }
    wakeFromSleepAllChips();
    LTC6804_wrcfg(TOTAL_IC, tx_cfg);
    // dischargeAll();
}


void poll_cell_voltage() {
    Serial.println("Polling Voltages...");
    /*
     * Difference between wakeup_sleep and wakeup_idle
     * wakeup_sleep wakes up the LTC6804 from sleep state
     * wakeup_idle wakes up the isoSPI port.
     */
    wakeFromSleepAllChips();
    LTC6804_wrcfg(TOTAL_IC, tx_cfg);
    wakeFromIdleAllChips();
    LTC6804_adcv();
    delay(10);
    wakeup_idle();
    uint8_t error = LTC6804_rdcv(0, TOTAL_IC, cell_voltages); // asks chip to read voltages and sto in given array.
    if (error == -1) {
        Serial.println("A PEC error was detected in cell voltage data");
    }
    printCells(); // prints the cell voltages to Serial.
    delay(200); // TODO: Why 200 milliseconds?
}

void process_voltages() {
    poll_cell_voltage(); // cell_voltages[] array populated with cell voltages now.
    double totalVolts = 0; // stored as double volts
    uint16_t maxVolt = cell_voltages[0][0]; // stored in 0.1 mV units
    uint16_t minVolt = cell_voltages[0][0]; // stored in 0.1 mV units
    double avgVolt = 0; // stored as double volts
    int maxIC = 0;
    int maxCell = 0;
    int minIC = 0;
    int minCell = 0;
    for (int ic = 0; ic < TOTAL_IC; ic++) {
        for (int cell = 0; cell < TOTAL_CELLS; cell++) {
            if ((ic != 0 || cell != 4) && (ic != 1 || cell != 7)&&!ignore_cell[ic][cell]) {
                uint16_t currentCell = cell_voltages[ic][cell];
                cell_delta_voltage[ic][cell] = currentCell - cell_delta_voltage[ic][cell];
                if (currentCell > maxVolt) {
                    maxVolt = currentCell;
                    maxIC = ic;
                    maxCell = cell;
                }
                if (currentCell < minVolt) {
                    minVolt = currentCell;
                    minIC = ic;
                    minCell = cell;
                }
                totalVolts += currentCell * 0.0001;
            }
        }
    }
    avgVolt = totalVolts / (TOTAL_IC * TOTAL_CELLS); // stored as double volts
    bmsVoltageMessage.setAverage(static_cast<uint16_t>(avgVolt * 1000 + 0.5)); // stored in millivolts
    bmsVoltageMessage.setTotal(static_cast<uint16_t>(totalVolts + 0.5)); // number is in units volts
    minVolt = minVolt + 5;
    maxVolt = maxVolt + 5;
    bmsVoltageMessage.setLow(minVolt);
    bmsVoltageMessage.setHigh(maxVolt);

    // TODO: Low and High voltage error checking.
    if (bmsVoltageMessage.getHigh() > voltage_cutoff_high) {
        bmsStatusMessage.setOvervoltage(true);
        Serial.println("VOLTAGE FAULT!!!!!!!!!!!!!!!!!!!");
        Serial.print("max IC: "); Serial.println(maxIC);
        Serial.print("max Cell: "); Serial.println(maxCell); Serial.println();
    }

    if (bmsVoltageMessage.getLow() < voltage_cutoff_low) {
        bmsStatusMessage.setUndervoltage(true);
        Serial.println("VOLTAGE FAULT!!!!!!!!!!!!!!!!!!!");
        Serial.print("min IC: "); Serial.println(minIC);
        Serial.print("min Cell: "); Serial.println(minCell); Serial.println();
    }
    if (bmsVoltageMessage.getTotal() > total_voltage_cutoff) {
        bmsStatusMessage.setTotalVoltageHigh(true);
        Serial.println("VOLTAGE FAULT!!!!!!!!!!!!!!!!!!!");
    }

    Serial.print("Avg: "); Serial.println(avgVolt, 4);
    Serial.print("Total: "); Serial.println(totalVolts, 4);
    Serial.print("Min: "); Serial.println(minVolt);
    Serial.print("Max: "); Serial.println(maxVolt);
}


float process_current() {
    // max positive current at 90% of 5V = 4.5V
    // max negative current in opposite direction at 10% of 5V = 0.5V
    // 0 current at 50% of 5V = 2.5V
    // max current sensor reading +/- 300A
    // current = 300 * (V - 2.5v) / 2v
    double senseVoltage = analogRead(CURRENT_SENSE) * 5.0 / 1024;
    float current = (float) max_val_current_sense * (senseVoltage - 2.5) / 2;
    Serial.print("Current: "); Serial.println(current);
    bmsCurrentMessage.setCurrent(current);
    if (current < 0) {
        bmsCurrentMessage.setChargingState(CHARGING);
        if (bmsCurrentMessage.getCurrent() < charge_current_constant_high) {
            bmsStatusMessage.setChargeOvercurrent(true);
            Serial.println("CHARGE CURRENT HIGH FAULT!!!!!!!!!!!!!!!!!!!");
        }
    } else if (current > 0) {
        bmsCurrentMessage.setChargingState(DISCHARGING);
        if (bmsCurrentMessage.getCurrent() > discharge_current_constant_high) {
            bmsStatusMessage.setDischargeOvercurrent(true);
            Serial.println("DISCHARGE CURRENT HIGH FAULT!!!!!!!!!!!!!!!!!!!");
        }
    }
    return current;
}

void wakeFromSleepAllChips() {
    for (int i = 0; i < TOTAL_IC / 3; i++) {
        wakeup_sleep();
//        delay(3);
    }
}

void wakeFromIdleAllChips() {
    for (int i = 0; i < TOTAL_IC / 3; i++) {
        wakeup_idle();
//        delay(3);
    }
}


void printCells() {
    for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
        Serial.print("IC: ");
        Serial.println(current_ic+1);
        for (int i = 0; i < TOTAL_CELLS; i++) {
            Serial.print("C"); Serial.print(i);
            if (ignore_cell[current_ic][i]) {
                Serial.print(" IGNORED CELL ");
            }
            Serial.print(": ");
            float voltage = cell_voltages[current_ic][i] * 0.0001;
            Serial.println(voltage, 4);
        }
        Serial.println();
    }
}
