/*
 * HyTech 2018 BMS Control Unit
 * Init 2017-04-11
 * Configured for HV Board Rev 2
 * Monitors cell voltages and temperatures, sends BMS_OK signal to close Shutdown Circuit
 */

/*
 * Shutdown circuit notes:
 * 1. GLV control system latches shutdown circuit closed.
 * 2. AIR's close.
 * 3. High voltage is available to the motor controller, TSAL is lit.
 * 4. Any faults (IMD OKHS, BMS_OK, BSPD) will open shutdown circuit, opening AIR's.
 */

/*
 * Operation notes:
 * 1. BMS sensors can be powered at all times.
 * 2. Once Teensy gets power from external power lines, give BMS_OK signal.
 * 3. No need to check DC bus voltage, because all batteries read their true voltages at all times. (They are continuous with each other at all times, due to no relay.)
 * 4. Once temperatures go too high, current goes too high, or cell voltages go too high or too low, drive the BMS_OK signal low.
 */

/*
 * LTC6804 state / communication notes:
 * The operation of the LTC6804 is divided into two separate sections: the core circuit and the isoSPI circuit. Both sections have an independent set of operating states, as well as a shutdown timeout. See LTC6804 Datasheet Page 20.
 * When sending an ADC conversion or diagnostic command, wake up the core circuit by calling wakeup_sleep()
 * When sending any other command (such as reading or writing registers), wake up the isoSPI circuit by calling wakeup_idle().
 */

#include <Arduino.h>
#include <Metro.h>
#include <FlexCAN.h>
#include "HyTech17.h"
#include "LT_SPI.h"
#include "LTC68042.h"
#include "ADC_SPI.h"

/*
 * Pin definitions
 */
#define BMS_OK A8
#define CURRENT_SENSE A3
#define LED_STATUS 7
#define LTC6820_CS 10
#define TEMP_SENSE_1 A9
#define TEMP_SENSE_2 A2
#define WATCHDOG A0

/*
 * Constant definitions
 */
#define TOTAL_IC 6
#define CELLS_PER_IC 9
#define THERMISTORS_PER_IC 3
#define PCB_THERM_PER_IC 2
#define TOTAL_CELLS 18 // Number of non-ignored cells (used for calculating averages)
#define TOTAL_PCB_THERMISTORS 12    // number of non-ignored PCB thermistors (for averages)
#define TOTAL_CELL_THERMISTORS 18   // number of non-ignored Cell thermistors (for averages)
#define THERMISTOR_RESISTOR_VALUE 10000
#define IGNORE_FAULT_THRESHOLD 5
#define SHUTDOWN_HIGH_THRESHOLD 2
#define DC_BUS_HIGH_THRESHOLD 50

/*
 * Current Sensor ADC Channel definitions
 */
#define ADC_CS 9
#define CH_CUR_SENSE_1 1
#define CH_CUR_SENSE_2 3
#define CH_TEMP_SENSE_1 2
#define CH_TEMP_SENSE_2 4
#define CH_SHUTDOWN 6
#define CH_5V 5

/*
 * Timers
 */
Metro timer_can_update = Metro(100);
Metro timer_debug = Metro(500);
Metro timer_process_cells = Metro(1000);
Metro timer_watchdog_timer = Metro(250);
Metro timer_charge_timeout = Metro(1000);

/*
 * Global variables
 */
uint16_t voltage_cutoff_low = 29800; // 2.9800V
uint16_t voltage_cutoff_high = 42000; // 4.2000V
uint16_t total_voltage_cutoff = 15000; // 150.00V
uint16_t discharge_current_constant_high = 22000; // 220.00A
uint16_t charge_current_constant_high = -10000; // 100.00A // TODO take into account max charge allowed for regen, 100A is NOT NOMINALLY ALLOWED!
uint16_t charge_temp_cell_critical_high = 4400; // 44.00C
uint16_t discharge_temp_cell_critical_high = 6000; // 60.00C
uint16_t onboard_temp_balance_disable = 5000;  // TODO fill this in with real numbers
uint16_t onboard_temp_balance_reenable = 4000; // TODO fill this in with real numbers
uint16_t onboard_temp_critical_high = 6000; // TODO fill this in with real numbers
uint16_t temp_critical_low = 0; // 0C
uint16_t voltage_difference_threshold = 500; // 5.00V

uint8_t error_flags_history = 0; // will not be reset with BMS_OK if a fault has occured
uint8_t first_fault_overvoltage = 0;// keep track of how many consecutive faults occured
uint8_t first_fault_undervoltage = 0;
uint8_t first_fault_total_voltage_high = 0;
bool fh_watchdog_test = false;


uint16_t cell_voltages[TOTAL_IC][12]; // contains 12 battery cell voltages. Numbers are stored in 0.1 mV units.
uint16_t aux_voltages[TOTAL_IC][6]; // contains auxiliary pin voltages.
     /* Data contained in this array is in this format:
      * Thermistor 1
      * Thermistor 2
      * Thermistor 3
      */
int16_t cell_delta_voltage[TOTAL_IC][CELLS_PER_IC]; // keep track of which cells are being discharged
int8_t ignore_cell[TOTAL_IC][CELLS_PER_IC]; //cells to be ignored for Balance testing
int8_t ignore_pcb_therm[TOTAL_IC][PCB_THERM_PER_IC]; // PCB thermistors to be ignored
int8_t ignore_cell_therm[TOTAL_IC][THERMISTORS_PER_IC]; // Cell thermistors to be ignored

/*!<
  The tx_cfg[][6] store the LTC6804 configuration data that is going to be written
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
FlexCAN CAN(500000);
static CAN_message_t msg;

// ADC Declaration
ADC_SPI ADC(ADC_CS);

/**
 * BMS State Variables
 */
BMS_detailed_voltages bms_detailed_voltages[TOTAL_IC][3];
BMS_status bms_status;
BMS_temperatures bms_temperatures;
BMS_detailed_temperatures bms_detailed_temperatures[TOTAL_IC];
BMS_onboard_temperatures bms_onboard_temperatures;
BMS_onboard_detailed_temperatures bms_onboard_detailed_temperatures[TOTAL_IC];
BMS_voltages bms_voltages;
MC_voltage_information mc_voltage_info;
bool watchdog_high = true;
bool disable_balance = false;

int minVoltageICIndex;
int minVoltageCellIndex;
int voltage_difference;

bool cell_discharging[TOTAL_IC][12];

void setup() {
    pinMode(BMS_OK, OUTPUT);
    pinMode(CURRENT_SENSE, INPUT);
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LTC6820_CS,OUTPUT);
    pinMode(TEMP_SENSE_1,INPUT);
    pinMode(TEMP_SENSE_2,INPUT);
    pinMode(WATCHDOG, OUTPUT);

    Serial.begin(115200); // Init serial for PC communication
    CAN.begin(); // Init CAN for vehicle communication
    delay(100);
    Serial.println("CAN system and serial communication initialized");

    digitalWrite(BMS_OK, HIGH);
    digitalWrite(WATCHDOG, watchdog_high);

    initialize(); // Call our modified initialize function instead of the default Linear function
    init_cfg(); // Initialize and write configuration registers to LTC6804 chips
    poll_cell_voltage();
    memcpy(cell_delta_voltage, cell_voltages, 2 * TOTAL_IC * CELLS_PER_IC);
    bms_status.set_state(BMS_STATE_DISCHARGING);
    Serial.println("Setup Complete!");

    // DEBUG Code for testing cell packs
    for (int i=0; i<4; i++) {
        for (int j=0; j<9; j++) {
            ignore_cell[i][j] = true; // Ignore ICs 0-3
        }
    }
    // DEBUG insert PCB thermistors to ignore here
    // DEBUG insert cell thermistors to ignore here
}

// TODO Implement Coulomb counting to track state of charge of battery.

/*
 * Main BMS Control Loop
 */
void loop() {
    while (CAN.read(msg)) {
        //Serial.println("reading CAN message");
        //Serial.println(msg.id, HEX);
        // if (msg.id == ID_BMS_TEMPERATURES) { // Used temporarily while we have an external temperature monitor ECU
        //     bms_temperatures.load(msg.buf);
        //     //bms_status.set_discharge_overtemp(false);  // RESET these values, then check below if they should be set again
        //     //bms_status.set_charge_overtemp(false);
        //     //bms_status.set_undertemp(false);
        //     if (bms_status.get_state() == BMS_STATE_DISCHARGING && bms_temperatures.get_high_temperature() > discharge_temp_cell_critical_high) {
        //         bms_status.set_discharge_overtemp(true);
        //         Serial.println("Discharge overtemperature fault!");
        //     } else if (bms_status.get_state() >= BMS_STATE_CHARGE_ENABLED && bms_temperatures.get_high_temperature() > charge_temp_cell_critical_high) {
        //         bms_status.set_charge_overtemp(true);
        //         Serial.println("Charge overtemperature fault!");
        //     } else if (bms_temperatures.get_low_temperature() < temp_critical_low) {
        //         bms_status.set_undertemp(true);
        //         Serial.println("Undertemperature fault!");
        //     }
        // }
        if (msg.id == ID_CCU_STATUS) { // Check if CCU status message is received and enable charger
            bms_status.set_state(BMS_STATE_CHARGE_ENABLED);
            timer_charge_timeout.reset();
        }
        if (timer_charge_timeout.check()) { // 1 second timeout - if timeout is reached, disable charging
            bms_status.set_state(BMS_STATE_DISCHARGING);
        }

        if (msg.id == ID_FH_WATCHDOG_TEST) { // stop sending pulse to watchdog timer
            fh_watchdog_test = true;
            //Serial.println("FH watchdog test");
        }

        // Need MC voltage info to determine if MC is receiving tractive system voltage
        if (msg.id == ID_MC_VOLTAGE_INFORMATION) {
            mc_voltage_info.load(msg.buf);
        }
    }

    if (timer_process_cells.check()) {
        process_voltages(); // polls controller, and store data in bms_voltages object.
        //bms_voltages.set_low(37408); // DEBUG Remove before final code
        //balance_cells();
        process_cell_temps(); // store data in bms_temperatures and bms_detailed_temperatures
        process_onboard_temps(); // store data in bms_onboard_temperatures and bms_onboard_detailed_temperatures
        //process_current(); // store data in bms_status object.
        print_uptime();
        print_temperatures();

        if (bms_status.get_error_flags()) { // BMS error - drive BMS_OK signal low
            Serial.println("STATUS NOT GOOD!!!!!!!!!!!!!!!");
            Serial.print("Error code: 0x");
            Serial.println(bms_status.get_error_flags(), HEX);
            error_flags_history = bms_status.get_error_flags();

            digitalWrite(BMS_OK, LOW);
        }else {
            digitalWrite(BMS_OK, HIGH);
            if(error_flags_history > 0){
                Serial.println("An Error Occured But Has Been Cleared");
                Serial.print("Error code: 0x");
                Serial.println(error_flags_history, HEX);
            }
        }
    }

    /*
    uint8_t data[1][8];
    uint8_t cmd[2] = {0x00,0x12};
    int res = LTC6804_rdcfg(0,data);
    for (int i = 0;i<8;i++){
      Serial.print("register: ");
      Serial.print(i);
      Serial.print(" value: ");
      Serial.println(data[0][i]);

       Serial.print("PEC:");
       Serial.println(res);
    }
    delay(2000);
    */

    if (timer_can_update.check()) {
        bms_status.write(msg.buf);
        msg.id = ID_BMS_STATUS;
        msg.len = sizeof(CAN_message_bms_status_t);
        CAN.write(msg);

        bms_voltages.write(msg.buf);
        msg.id = ID_BMS_VOLTAGES;
        msg.len = sizeof(CAN_message_bms_voltages_t);
        CAN.write(msg);

        msg.id = ID_BMS_DETAILED_VOLTAGES;
        msg.len = sizeof(CAN_message_bms_detailed_voltages_t);
        for (int i = 0; i < TOTAL_IC; i++) {
            for (int j = 0; j < 3; j++) {
                bms_detailed_voltages[i][j].write(msg.buf);
                CAN.write(msg);
            }
        }

        /*bms_temperatures.write(msg.buf);
        msg.id = ID_BMS_TEMPERATURES;
        msg.len = sizeof(CAN_message_bms_temperatures_t);
        CAN.write(msg);*/
    }

    if (timer_watchdog_timer.check() && !fh_watchdog_test) { // Send alternating keepalive signal to watchdog timer   
        watchdog_high = !watchdog_high;
        digitalWrite(WATCHDOG, watchdog_high);
        /*Serial.print("set watchdog timer ");
        Serial.print(watchdog_high);
        Serial.print(" ");
        Serial.println(digitalRead(BMS_OK));*/
    }
}

/*
 * Initialize communication with LTC6804 chips. Based off of LTC6804_initialize()
 * Changes: Doesn't call quikeval_SPI_connect(), Sets ADC mode to MD_FILTERED
 */
void initialize() {
    spi_enable(SPI_CLOCK_DIV16); // Sets 1MHz Clock
    set_adc(MD_FILTERED,DCP_DISABLED,CELL_CH_ALL,AUX_CH_ALL); // Sets global variables used in ADC commands // TODO Change CELL_CH_ALL and AUX_CH_ALL so we don't read all GPIOs and cells
}

/*
 * Initialize the configuration array and write configuration to ICs
 * See LTC6804 Datasheet Page 51 for tables of register definitions
 */
void init_cfg() {
    for (int i = 0; i < TOTAL_IC; i++) {
        tx_cfg[i][0] = 0xFE; // 11111110 - All GPIOs enabled, Reference Remains Powered Up Until Watchdog Timeout, ADCOPT 0 allows us to use filtered ADC mode // TODO maybe we can speed things up by disabling some GPIOs
        tx_cfg[i][1] = 0x00;
        tx_cfg[i][2] = 0x00;
        tx_cfg[i][3] = 0x00;
        tx_cfg[i][4] = 0x00;
        tx_cfg[i][5] = 0x00;
    }
    cfg_set_overvoltage_comparison_voltage(voltage_cutoff_high * 10); // Calculate overvoltage comparison register values
    cfg_set_undervoltage_comparison_voltage(voltage_cutoff_low * 10); // Calculate undervoltage comparison register values
    wakeup_idle(); // Wake up isoSPI
    delayMicroseconds(1200); // Wait 4*t_wake for wakeup command to propogate and all 4 chips to wake up - See LTC6804 Datasheet page 54
    LTC6804_wrcfg(TOTAL_IC, tx_cfg); // Write configuration to ICs
}

void discharge_cell(int ic, int cell) {
    cell_discharging[ic][cell] = true;
    discharge_cell(ic, cell, true);
}

void discharge_cell(int ic, int cell, bool setDischarge) {
    if (ic < TOTAL_IC && cell < CELLS_PER_IC) {
        if (cell > 4) {
            cell++; // Increment cell, skipping the disconnected C5. This abstracts the missing cell from the rest of the program.
        }
        if (cell < 8) {
            if(setDischarge) {
                tx_cfg[ic][4] = tx_cfg[ic][4] | (0b1 << cell); 
            } else {
                tx_cfg[ic][4] = tx_cfg[ic][4] & ~(0b1 << cell ); 
            }
        } else {
            if (setDischarge) {
                tx_cfg[ic][5] = tx_cfg[ic][5] | (0b1 << (cell - 8)); 
            } else {
                tx_cfg[ic][5] = tx_cfg[ic][5] & ~(0b1 << (cell - 8)); 
            }
        }
    }
    wakeup_idle();
    //delayMicroseconds(1200); // Wait 4*t_wake for wakeup command to propogate and all 4 chips to wake up - See LTC6804 Datasheet page 54
    LTC6804_wrcfg(TOTAL_IC, tx_cfg);
}

void discharge_all() {
    for (int i = 0; i < TOTAL_IC; i++) {
        tx_cfg[i][4] = 0b11111111;
        tx_cfg[i][5] = tx_cfg[i][5] | 0b00001111;
    }
    wakeup_idle();
    //delayMicroseconds(1200); // Wait 4*t_wake for wakeup command to propogate and all 4 chips to wake up - See LTC6804 Datasheet page 54
    LTC6804_wrcfg(TOTAL_IC, tx_cfg);
}

void stop_discharge_cell(int ic, int cell) {
    cell_discharging[ic][cell] = false;
    discharge_cell(ic, cell, false);
}

void stop_discharge_all() {
    for (int i = 0; i < TOTAL_IC; i++) {
        tx_cfg[i][4] = 0b0;
        tx_cfg[i][5] = 0b0;
    }
    wakeup_idle();
    //delayMicroseconds(1200); // Wait 4*t_wake for wakeup command to propogate and all 4 chips to wake up - See LTC6804 Datasheet page 54
    LTC6804_wrcfg(TOTAL_IC, tx_cfg);
}

void balance_cells() {
    if (bms_voltages.get_low() > voltage_cutoff_low 
        && mc_voltage_info.get_dc_bus_voltage() >= DC_BUS_HIGH_THRESHOLD
        && !disable_balance
        && bms_status.get_state() >= BMS_STATE_CHARGE_ENABLED) {

        for (int ic = 0; ic < TOTAL_IC; ic++) { // Loop through ICs
            for (int cell = 0; cell < CELLS_PER_IC; cell++) { // Loop through cells
                if (!ignore_cell[ic][cell]) { // Ignore any cells specified in ignore_cell
                    uint16_t cell_voltage = cell_voltages[ic][cell]; // current cell voltage in mV
                    if (cell_discharging[ic][cell]) {
                        if (cell_voltage < bms_voltages.get_low() + voltage_difference_threshold - 6) {
                            stop_discharge_cell(ic, cell);
                        }
                    }
                    else if (cell_voltage > bms_voltages.get_low() + voltage_difference_threshold) {
                            discharge_cell(ic, cell);
                    }
                }
            }
        }
    }
    else {
        Serial.println("Not Balancing!");
        stop_discharge_all(); // Make sure none of the cells are discharging
    }
}

void poll_cell_voltage() {
    //Serial.println("Polling Voltages...");
    wakeup_sleep(); // Wake up LTC6804 ADC core
    LTC6804_adcv(); // Start cell ADC conversion
    delay(205); // Need to wait at least 201.317ms for conversion to finish, due to filtered sampling mode (26Hz) - See LTC6804 Datasheet Table 5
    //wakeup_sleep(); // TODO testing since every once in a while we still get 6.5535 issue with below 2 lines
    wakeup_idle(); // Wake up isoSPI
    delayMicroseconds(1200); // Wait 4*t_wake for wakeup command to propogate and all 4 chips to wake up - See LTC6804 Datasheet page 54
    uint8_t error = LTC6804_rdcv(0, TOTAL_IC, cell_voltages); // Reads voltages from ADC registers and stores in cell_voltages.
    if (error == -1) {
        Serial.println("A PEC error was detected in cell voltage data");
    }
    // Move C7-C10 down by one in the array, skipping C6. This abstracts the missing cell from the rest of the program.
    for (int i=0; i<TOTAL_IC; i++) { // Loop through ICs
        for (int j=6; j<10; j++) { // Loop through C7-C10
            cell_voltages[i][j-1] = cell_voltages[i][j];
        }
    }
    print_cells(); // Print the cell voltages to Serial.
}

void process_voltages() {
    poll_cell_voltage(); // cell_voltages[] array populated with cell voltages after this
    uint32_t totalVolts = 0; // stored in 10 mV units
    uint16_t maxVolt = 0; // stored in 0.1 mV units
    uint16_t minVolt = 65535; // stored in 0.1 mV units
    uint16_t avgVolt = 0; // stored in 0.1 mV units
    int maxIC = 0;
    int maxCell = 0;
    int minIC = 0;
    int minCell = 0;
    for (int ic = 0; ic < TOTAL_IC; ic++) {
        for (int cell = 0; cell < CELLS_PER_IC; cell++) {
            bms_detailed_voltages[ic][cell / 3].set_voltage(cell % 3, cell_voltages[ic][cell]); // Populate CAN message struct
            if (!ignore_cell[ic][cell]) {
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
                totalVolts += currentCell;
            }
        }
    }
    avgVolt = totalVolts / TOTAL_CELLS; // stored in 0.1 mV units
    totalVolts /= 100; // convert 0.1mV units down to 10mV units
    bms_voltages.set_average(avgVolt);
    bms_voltages.set_low(minVolt);
    bms_voltages.set_high(maxVolt);
    bms_voltages.set_total(totalVolts);

    // TODO: Low and High voltage error checking.

    bms_status.set_overvoltage(false); // RESET these values, then check below if they should be set again
    bms_status.set_undervoltage(false);
    bms_status.set_total_voltage_high(false);

    if (bms_voltages.get_high() > voltage_cutoff_high) {
        if (first_fault_overvoltage==IGNORE_FAULT_THRESHOLD) {
            bms_status.set_overvoltage(true); // TODO may need to comment this out for now when driving due to 65535 errors
        } else {
            first_fault_overvoltage += 1;
        }
        Serial.println("VOLTAGE FAULT too high!!!!!!!!!!!!!!!!!!!");
        Serial.print("max IC: "); Serial.println(maxIC);
        Serial.print("max Cell: "); Serial.println(maxCell); Serial.println();
    } else {
        first_fault_overvoltage = 0;
    }

    if (bms_voltages.get_low() < voltage_cutoff_low) {
        if (first_fault_undervoltage==IGNORE_FAULT_THRESHOLD) {
            bms_status.set_undervoltage(true);
        }else {
            first_fault_undervoltage += 1;
        }
        Serial.println("VOLTAGE FAULT too low!!!!!!!!!!!!!!!!!!!");
        Serial.print("min IC: "); Serial.println(minIC);
        Serial.print("min Cell: "); Serial.println(minCell); Serial.println();
    } else {
        first_fault_undervoltage = 0;
    }

    if (bms_voltages.get_total() > total_voltage_cutoff) {
        if (first_fault_total_voltage_high==IGNORE_FAULT_THRESHOLD) {
            bms_status.set_total_voltage_high(true); // TODO may need to comment this out for now when driving due to 65535 errors
        }else {
            first_fault_total_voltage_high += 1;
        }
        Serial.println("VOLTAGE FAULT!!!!!!!!!!!!!!!!!!!");
    }else{ 
        first_fault_total_voltage_high = 0;
    }

    Serial.print("Average: "); Serial.println(avgVolt / (double) 1e4, 4);
    Serial.print("Total: "); Serial.println(totalVolts / (double) 1e2, 4);
    Serial.print("Min: "); Serial.println(minVolt / (double) 1e4, 4);
    Serial.print("Max: "); Serial.println(maxVolt / (double) 1e4, 4);
    Serial.println();
}

void poll_aux_voltage() {
    wakeup_sleep();
    //delayMicroseconds(200) // TODO try this if we are still having intermittent 6.5535 issues, maybe the last ADC isn't being given enough time to wake up
    LTC6804_adax(); // Start GPIO ADC conversion
    delay(205); // Need to wait at least 201.317ms for conversion to finish, due to filtered sampling mode (26Hz) - See LTC6804 Datasheet Table 5
    wakeup_idle(); // Wake up isoSPI
    delayMicroseconds(1200); // Wait 4*t_wake for wakeup command to propogate and all 4 chips to wake up - See LTC6804 Datasheet page 54
    uint8_t error = LTC6804_rdaux(0, TOTAL_IC, aux_voltages);
    if (error == -1) {
        Serial.println("A PEC error was detected in auxiliary voltage data");
    }
    // print_aux();
    delay(200);
}

void process_cell_temps() { // TODO make work with signed int8_t CAN message (yes temperatures can be negative)
    double avgTemp, lowTemp, highTemp, totalTemp, thermTemp;
    poll_aux_voltage();
    totalTemp = 0;
    lowTemp = calculate_cell_temp(aux_voltages[0][0], aux_voltages[0][5]);
    highTemp = lowTemp;
    for (int ic = 0; ic < TOTAL_IC; ic++) {
        for (int j = 0; j < THERMISTORS_PER_IC; j++) {
            if (!ignore_cell_therm[ic][j]) {
                thermTemp = calculate_cell_temp(aux_voltages[ic][j], aux_voltages[ic][5]); // TODO: replace 3 with aux_voltages[ic][5]?
                
                if (thermTemp < lowTemp) {
                    lowTemp = thermTemp;
                }
                
                if (thermTemp > highTemp) {
                    highTemp = thermTemp;
                }
                
                bms_detailed_temperatures[ic].set_temperature(j, thermTemp); // Populate CAN message struct
                totalTemp += thermTemp;

                Serial.print("Thermistor ");
                Serial.print(j);
                Serial.print(": ");
                Serial.print(thermTemp / 100, 2);
                Serial.println(" C");
            } else {
                Serial.print("Ignored thermistor ");
                Serial.println(j);
            }
        }
        Serial.println("----------------------\n");
    }
    avgTemp = (int16_t) (totalTemp / TOTAL_CELL_THERMISTORS);
    bms_temperatures.set_low_temperature((int16_t) lowTemp);
    bms_temperatures.set_high_temperature((int16_t) highTemp);
    bms_temperatures.set_average_temperature((int16_t) avgTemp);

    bms_status.set_discharge_overtemp(false); // RESET these values, then check below if they should be set again
    bms_status.set_charge_overtemp(false);

    if (bms_status.get_state() == BMS_STATE_DISCHARGING) { // Discharging
        if (bms_temperatures.get_high_temperature() > discharge_temp_cell_critical_high) {
            bms_status.set_discharge_overtemp(true);
            Serial.println("TEMPERATURE FAULT!!!!!!!!!!!!!!!!!!!");
        }
    } else if (bms_status.get_state() >= BMS_STATE_CHARGE_ENABLED) { // Charging
        if (bms_temperatures.get_high_temperature() > charge_temp_cell_critical_high) {
            bms_status.set_charge_overtemp(true);
            Serial.println("TEMPERATURE FAULT!!!!!!!!!!!!!!!!!!!");
        }
    }

    Serial.print("Low Temp: ");
    Serial.println(lowTemp / 100);
    Serial.print("High Temp: ");
    Serial.println(highTemp / 100);
    Serial.print("Average Temp: ");
    Serial.println(avgTemp / 100);
}

double calculate_cell_temp(double aux_voltage, double v_ref) {
   /* aux_voltage = (R/(10k+R))*v_ref 
    * R = 10k * aux_voltage / (v_ref - aux_voltage)
    */
    aux_voltage /= 10000;
    //Serial.print("voltage: ");
    //Serial.println(aux_voltage);
    v_ref /= 10000;
    //Serial.print("v ref: ");
    //Serial.println(v_ref); 
    double thermistor_resistance = 1e4 * aux_voltage / (v_ref - aux_voltage); 
    //Serial.print("thermistor resistance: ");
    //Serial.println(thermistor_resistance);  
   /*
     * Temperature equation (in Kelvin) based on resistance is the following:
     * 1/T = 1/T0 + (1/B) * ln(R/R0)      (R = thermistor resistance)
     * T = 1/(1/T0 + (1/B) * ln(R/R0))
     */
     double T0 = 298.15; // 25C in Kelvin
     double b = 3984;    // B constant of the thermistor
     double R0 = 10000;  // Resistance of thermistor at 25C
     double temperature = 1 / ((1 / T0) + (1 / b) * log(thermistor_resistance / R0)) - (double) 273.15;
     //Serial.print("temperature: ");
     //Serial.println(temperature); 
     return (int16_t)(temperature * 100);
}

void process_onboard_temps() {
    Serial.println("Process onboard temps");
    double avgTemp, lowTemp, highTemp, totalTemp, thermTemp;
    poll_aux_voltage();
    totalTemp = 0;
    lowTemp = calculate_onboard_temp(aux_voltages[0][3], aux_voltages[0][5]);
    highTemp = lowTemp;

    for (int ic = 0; ic < TOTAL_IC; ic++) {
        for (int j = 0; j < PCB_THERM_PER_IC; j++) {
            if (!ignore_pcb_therm[ic][j]) {
                thermTemp = calculate_onboard_temp(aux_voltages[ic][j+3], aux_voltages[ic][5]);
                if (thermTemp < lowTemp) {
                    lowTemp = thermTemp;
                }
                
                if (thermTemp > highTemp) {
                    highTemp = thermTemp;
                }
                
                bms_onboard_detailed_temperatures[ic].set_temperature(j, thermTemp * 10000); // Populate CAN message struct
                totalTemp += thermTemp;

                Serial.print("PCB thermistor ");
                Serial.print(j);
                Serial.print(": ");
                Serial.print(thermTemp / 100, 2);
                Serial.println(" C"); 
            } else {
                Serial.print("Ignored PCB thermistor ");
                Serial.println(j);
            }
        }
        Serial.println("----------------------\n");
    }
    avgTemp = (int16_t) (totalTemp / ((TOTAL_IC) * PCB_THERM_PER_IC));
    bms_onboard_temperatures.set_low_temperature((int16_t) lowTemp);
    bms_onboard_temperatures.set_high_temperature((int16_t) highTemp);
    bms_onboard_temperatures.set_average_temperature((int16_t) avgTemp);

    // TODO don't fault the BMS based on onboard thermistors, but do disable balancing

    bms_status.set_onboard_overtemp(false); // RESET this value, then check below if they should be set

    //marker
    if (bms_status.get_state() == BMS_STATE_DISCHARGING) { // Discharging
        if (bms_onboard_temperatures.get_high_temperature() > onboard_temp_critical_high) {
            bms_status.set_onboard_overtemp(true);
            Serial.println("TEMPERATURE FAULT!!!!!!!!!!!!!!!!!!!");
        }
    } else if (bms_status.get_state() >= BMS_STATE_CHARGE_ENABLED) { // Charging
        if (bms_onboard_temperatures.get_high_temperature() > onboard_temp_critical_high) {
            bms_status.set_onboard_overtemp(true);
            disable_balance = true;
            Serial.println("TEMPERATURE FAULT!!!!!!!!!!!!!!!!!!!");
        } else if (bms_onboard_temperatures.get_high_temperature() >= onboard_temp_balance_disable) {
            bms_status.set_state(BMS_STATE_CHARGE_DISABLED);
            disable_balance = true;
            Serial.println("WARNING: Onboard temperature too high; disabling balancing");
        }
    } else if (bms_status.get_state() == BMS_STATE_CHARGE_DISABLED) {
        if (bms_onboard_temperatures.get_high_temperature() < onboard_temp_balance_reenable) {
            bms_status.set_state(BMS_STATE_CHARGE_ENABLED);
            disable_balance = false;
            Serial.println("CLEARED: Onboard temperature OK; reenabling balancing");
        }
    }

    Serial.print("Low Temp: ");
    Serial.println(lowTemp / 100);
    Serial.print("High Temp: ");
    Serial.println(highTemp / 100);
    Serial.print("Average Temp: ");
    Serial.println(avgTemp / 100);
    return;
}

double calculate_onboard_temp(double aux_voltage, double v_ref) {
   /* aux_voltage = (R/(10k+R))*v_ref 
    * R = 10k * aux_voltage / (v_ref - aux_voltage)
    */
    aux_voltage /= 10000;
    //Serial.print("onboard voltage: ");
    //Serial.println(aux_voltage);
    v_ref /= 10000;
    //Serial.print("v ref: ");
    //Serial.println(v_ref); 
    double thermistor_resistance = 1e4 * aux_voltage / (v_ref - aux_voltage); 
    /*Serial.print("thermistor resistance: ");
    Serial.println(thermistor_resistance); */
   /*
     * Temperature equation (in Kelvin) based on resistance is the following:
     * 1/T = 1/T0 + (1/B) * ln(R/R0)      (R = thermistor resistance)
     * T = 1/(1/T0 + (1/B) * ln(R/R0))
     */
     double T0 = 298.15; // 25C in Kelvin
     double b = 3380;    // B constant of the thermistor
     double R0 = 10000;  // Resistance of thermistor at 25C
     double temperature = 1 / ((1 / T0) + (1 / b) * log(thermistor_resistance / R0)) - (double) 273.15;
     return (int16_t)(temperature * 100);
}

void process_current() {
    /*
     * Current sensor: ISB-300-A-604
     * Maximum positive current (300A) corresponds to 4.5V signal
     * Maximum negative current (-300A) corresponds to 0.5V signal
     * 0A current corresponds to 2.5V signal
     * 
     * voltage = read_adc() * 5 / 4095
     * current = (voltage - 2.5) * 300 / 2
     */
    double voltage = ADC.read_adc(CH_CUR_SENSE_1) / (double) 819;
    double current = (voltage - 2.5) * (double) 150;
    Serial.print("\nCurrent Sensor: ");
    Serial.print(current, 2);
    Serial.println("A");
    bms_status.set_current((int16_t) (current * 100));
    bms_status.set_charge_overcurrent(false); // RESET these values, then check below if they should be set again
    bms_status.set_discharge_overcurrent(false);
    if (bms_status.get_state() >= BMS_STATE_CHARGE_ENABLED) {
        if (bms_status.get_current() < charge_current_constant_high) {
            bms_status.set_charge_overcurrent(true);
            Serial.println("CHARGE CURRENT HIGH FAULT!!!!!!!!!!!!!!!!!!!");
        }
    } else {
        if (bms_status.get_current() > discharge_current_constant_high) {
            bms_status.set_discharge_overcurrent(true);
            Serial.println("DISCHARGE CURRENT HIGH FAULT!!!!!!!!!!!!!!!!!!!");
        }
    }
}

/*
 * Update maximum and minimum allowed voltage, current, temperature, etc.
 */
int update_constraints(uint8_t address, uint16_t value) {
    switch(address) {
        case 0: // voltage_cutoff_low
            voltage_cutoff_low = value;
            break;
        case 1: // voltage_cutoff_high
            voltage_cutoff_high = value;
            break;
        case 2: // total_voltage_cutoff
            total_voltage_cutoff = value;
            break;
        case 3: // discharge_current_constant_high
            discharge_current_constant_high = value;
            break;
        case 4: // charge_current_constant_high
            charge_current_constant_high = value;
            break;
        case 5: // charge_temp_cell_critical_high
            charge_temp_cell_critical_high = value;
            break;
        case 6: // discharge_temp_cell_critical_high
            discharge_temp_cell_critical_high = value;
            break;
        case 7: // voltage_difference_threshold
            voltage_difference_threshold = value;
            break;
        default:
            return -1;
    }
    return 0;
}

void print_cells() {
    for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
        Serial.print("IC: ");
        Serial.println(current_ic);
        for (int i = 0; i < CELLS_PER_IC; i++) {
            Serial.print("C"); Serial.print(i);
            if (ignore_cell[current_ic][i]) {
                Serial.print(" IGNORED CELL");
            }
            Serial.print(": ");
            float voltage = cell_voltages[current_ic][i] * 0.0001;
            Serial.print(voltage, 4);
            Serial.print(" Discharging: ");
            Serial.print(cell_discharging[current_ic][i]); 
            Serial.print(" Voltage difference: ");
            Serial.print(cell_voltages[current_ic][i]-bms_voltages.get_low());
            Serial.print(" Delta To Threshold: ");
            Serial.println((cell_voltages[current_ic][i]-bms_voltages.get_low())-voltage_difference_threshold);
        }
        Serial.println();
    }
}

void print_aux() {
    for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
        Serial.print("IC: ");
        Serial.println(current_ic);
        for (int i = 0; i < 6; i++) {
            Serial.print("Aux-"); Serial.print(i+1); Serial.print(": ");
            float voltage = aux_voltages[current_ic][i] * 0.0001;
            Serial.println(voltage, 4);
        }
        Serial.println();
    }
}

void print_temperatures() {
    Serial.print("\nAverage temperature: ");
    Serial.print(bms_temperatures.get_average_temperature() / (double) 100, 2);
    Serial.println(" C");
    Serial.print("Low temperature: ");
    Serial.print(bms_temperatures.get_low_temperature() / (double) 100, 2);
    Serial.println(" C");
    Serial.print("High temperature: ");
    Serial.print(bms_temperatures.get_high_temperature() / (double) 100, 2);
    Serial.println(" C\n");
}

/*
 * Print ECU uptime
 */
void print_uptime() {
    Serial.print("\nECU uptime: ");
    Serial.print(millis() / 1000);
    Serial.print(" seconds (");
    Serial.print(millis() / 1000 / 60);
    Serial.print(" minutes, ");
    Serial.print(millis() / 1000 % 60);
    Serial.println(" seconds)\n");
}

/*
 * Set VOV in configuration registers
 * Voltage is in 100 uV increments
 * See LTC6804 datasheet pages 25 and 53
 */
void cfg_set_overvoltage_comparison_voltage(uint16_t voltage) {
    voltage /= 16;
    for (int i = 0; i < TOTAL_IC; i++) {
        tx_cfg[i][2] = (tx_cfg[i][2] & 0x0F) | ((voltage && 0x00F) << 8);
        tx_cfg[i][3] = (voltage && 0xFF0) >> 4;
    }
}

/*
 * Set VUV in configuration registers
 * Voltage is in 100 uV increments
 * See LTC6804 datasheet pages 25 and 53
 */
void cfg_set_undervoltage_comparison_voltage(uint16_t voltage) {
    voltage /= 16;
    voltage -= 10000;
    for (int i = 0; i < TOTAL_IC; i++) {
        tx_cfg[i][1] = voltage && 0x0FF;
        tx_cfg[i][2] = (tx_cfg[i][2] & 0xF0) | ((voltage && 0xF00) >> 8);
    }
}
