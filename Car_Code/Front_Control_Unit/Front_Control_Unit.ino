/*
 * HyTech 2018 Vehicle Front Control Unit
 * Interface with dashboard lights, buttons, and buzzer.
 * Read pedal sensor values and communicate with motor controller.
 * Configured for Front ECU Board rev5
 */
#include <FlexCAN.h>
#include "HyTech17.h"
#include <Metro.h>

/*
 * Pin definitions
 */
#define ADC_ACCEL_1_CHANNEL 0
#define ADC_ACCEL_2_CHANNEL 1
#define ADC_BRAKE_CHANNEL 2
#define ADC_SPI_CS A0
#define ADC_SPI_DIN 0
#define ADC_SPI_DOUT 1
#define ADC_SPI_SCK 13
#define BTN_START A5
#define LED_START 5
#define LED_BMS 6
#define LED_IMD 7
#define READY_SOUND 2
#define SOFTWARE_SHUTDOWN_RELAY 12

/*
 * Constant definitions
 */
// TODO some of these values need to be calibrated once hardware is installed
#define BRAKE_ACTIVE 1600
#define MIN_ACCELERATOR_PEDAL_1 2394 // compare pedal travel
#define MAX_ACCELERATOR_PEDAL_1 1020
#define MIN_ACCELERATOR_PEDAL_2 372
#define MAX_ACCELERATOR_PEDAL_2 1372
#define MIN_BRAKE_PEDAL 1510
#define MAX_BRAKE_PEDAL 1684
#define MAX_TORQUE 1600 // Torque in Nm * 10
#define MIN_HV_VOLTAGE 500 // Volts in V * 0.1 - Used to check if Accumulator is energized

/*
 * Timers
 */
Metro timer_btn_start = Metro(10);
Metro timer_debug = Metro(200);
Metro timer_debug_raw_torque = Metro(200);
Metro timer_debug_torque = Metro(200);
Metro timer_ramp_torque = Metro(100);
Metro timer_inverter_enable = Metro(2000); // Timeout failed inverter enable
Metro timer_led_start_blink_fast = Metro(150);
Metro timer_led_start_blink_slow = Metro(400);
Metro timer_motor_controller_send = Metro(50);
Metro timer_ready_sound = Metro(2000); // Time to play RTD sound
Metro timer_can_update = Metro(100);

/*
 * Global variables
 */
FCU_status fcu_status;
FCU_readings fcu_readings;
RCU_status rcu_status;

bool btn_start_debouncing = false;
uint8_t btn_start_new = 0;
bool btn_start_pressed = false;
bool debug = true;
bool led_start_active = false;
uint8_t led_start_type = 0; // 0 for off, 1 for steady, 2 for fast blink, 3 for slow blink
float rampRatio = 1;

FlexCAN CAN(500000);
static CAN_message_t msg;

void setup() {
    pinMode(ADC_SPI_CS, OUTPUT);
    pinMode(ADC_SPI_DIN, INPUT);
    pinMode(ADC_SPI_DOUT, OUTPUT);
    pinMode(ADC_SPI_SCK, OUTPUT);
    pinMode(BTN_START, INPUT_PULLUP);
    pinMode(LED_BMS, OUTPUT);
    pinMode(LED_IMD, OUTPUT);
    pinMode(LED_START, OUTPUT);
    pinMode(READY_SOUND, OUTPUT);
    pinMode(SOFTWARE_SHUTDOWN_RELAY, OUTPUT);

    Serial.begin(115200); // Init serial for PC communication
    CAN.begin(); // Init CAN for vehicle communication
    delay(100);
    Serial.println("CAN system and serial communication initialized");

    digitalWrite(SOFTWARE_SHUTDOWN_RELAY, HIGH);
    set_state(FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE);

    // Send restart message, so RCU knows to power cycle the inverter (in case of CAN message timeout from FCU to inverter)
    msg.id = ID_FCU_RESTART;
    msg.len = 1;
    CAN.write(msg);
}

void loop() {
    while (CAN.read(msg)) {
        // Handle RCU status messages
        if (msg.id == ID_RCU_STATUS) {
            // Load message into RCU_status object
            rcu_status.load(msg.buf);
            digitalWrite(LED_BMS, !rcu_status.get_bms_ok_high());
            if (!rcu_status.get_bms_ok_high()) {
                Serial.println("RCU BMS FAULT: detected");
            }
            digitalWrite(LED_IMD, !rcu_status.get_imd_okhs_high());
            if (!rcu_status.get_imd_okhs_high()) {
                Serial.println("RCU IMD FAULT: detected");
            }
        }

        if (msg.id == ID_MC_VOLTAGE_INFORMATION) {
            MC_voltage_information mc_voltage_information = MC_voltage_information(msg.buf);
            if (mc_voltage_information.get_dc_bus_voltage() >= MIN_HV_VOLTAGE && fcu_status.get_state() == FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE) {
                set_state(FCU_STATE_TRACTIVE_SYSTEM_ACTIVE);
            }
            if (mc_voltage_information.get_dc_bus_voltage() < MIN_HV_VOLTAGE && fcu_status.get_state() > FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE) {
                set_state(FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE);
            }
        }

        if (msg.id == ID_MC_INTERNAL_STATES) {
            MC_internal_states mc_internal_states = MC_internal_states(msg.buf);
            if (mc_internal_states.get_inverter_enable_state() && fcu_status.get_state() == FCU_STATE_ENABLING_INVERTER) {
                set_state(FCU_STATE_WAITING_READY_TO_DRIVE_SOUND);
            }
        }
    }

    /*
     * Send state over CAN
     */
    if (timer_can_update.check()) {
        // Send Front Control Unit message
        fcu_status.set_accelerator_boost_mode(0);
        fcu_status.write(msg.buf);
        msg.id = ID_FCU_STATUS;
        msg.len = sizeof(CAN_message_fcu_status_t);
        CAN.write(msg);

        // Send second Front Control Unit message
        read_values(); // Calculate new values to send
        fcu_readings.write(msg.buf);
        msg.id = ID_FCU_READINGS;
        msg.len = sizeof(CAN_message_fcu_readings_t);
        CAN.write(msg);
    }

    /*
     * State machine
     */
    switch (fcu_status.get_state()) {
        case FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE:
        break;

        case FCU_STATE_TRACTIVE_SYSTEM_ACTIVE:
        if (btn_start_new == fcu_status.get_start_button_press_id()) { // Start button has been pressed
            if (fcu_status.get_brake_pedal_active()) { // Required to hold brake pedal to activate motor controller
                set_state(FCU_STATE_ENABLING_INVERTER);
            } else {
                btn_start_new = fcu_status.get_start_button_press_id() + 1;
            }
        }
        break;

        case FCU_STATE_ENABLING_INVERTER:
        if (timer_inverter_enable.check()) { // Inverter enable timeout
            set_state(FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE);
        }
        break;

        case FCU_STATE_WAITING_READY_TO_DRIVE_SOUND:
        if (timer_ready_sound.check()) { // RTDS has sounded
            set_state(FCU_STATE_READY_TO_DRIVE);
        }
        break;

        case FCU_STATE_READY_TO_DRIVE:
        if (timer_motor_controller_send.check()) {
            MC_command_message mc_command_message = MC_command_message(0, 0, 0, 1, 0, 0);
            read_values(); // Read new sensor values

            // Check for accelerator implausibility FSAE EV2.3.10
            fcu_status.set_accelerator_implausibility(false);
            /*if (fcu_readings.get_accelerator_pedal_raw_1() < MIN_ACCELERATOR_PEDAL_1 || fcu_readings.get_accelerator_pedal_raw_1() > MAX_ACCELERATOR_PEDAL_1) {
                fcu_status.set_accelerator_implausibility(true);
            }
            if (fcu_readings.get_accelerator_pedal_raw_2() < MIN_ACCELERATOR_PEDAL_2 || fcu_readings.get_accelerator_pedal_raw_2() > MAX_ACCELERATOR_PEDAL_2) {
                fcu_status.set_accelerator_implausibility(true);
            }*/

            // Calculate torque value
            int calculated_torque = 0;
            if (!fcu_status.get_accelerator_implausibility()) {
                int torque1 = map(fcu_readings.get_accelerator_pedal_raw_1(), MIN_ACCELERATOR_PEDAL_1, MAX_ACCELERATOR_PEDAL_1, 0, MAX_TORQUE);
                int torque2 = map(fcu_readings.get_accelerator_pedal_raw_2(), MIN_ACCELERATOR_PEDAL_2, MAX_ACCELERATOR_PEDAL_2, 0, MAX_TORQUE);
                /*if (abs(torque1 - torque2) * 100 / MAX_TORQUE > 10) { // Second accelerator implausibility check FSAE EV2.3.6
                fcu_status.set_accelerator_implausibility(true);
                } else {*/
                calculated_torque = (int) (min(torque1, torque2) * rampRatio);

                if (rampRatio < 1 && timer_ramp_torque.check()) {
                   rampRatio += 0.1;
                   if (rampRatio > 1) {
                      rampRatio = 1;
                   }
                }
                if (debug && timer_debug_raw_torque.check()) {
                    Serial.print("FCU RAW TORQUE: ");
                    Serial.println(calculated_torque);
                }
                if (calculated_torque > MAX_TORQUE) {
                    calculated_torque = MAX_TORQUE;
                }
                if (calculated_torque < 0) {
                    calculated_torque = 0;
                }
                /*}*/
            }

            // FSAE EV2.5 APPS / Brake Pedal Plausibility Check
            if (fcu_status.get_brake_implausibility() && !fcu_status.get_brake_pedal_active() && calculated_torque <= (MAX_TORQUE / 4)) {
                fcu_status.set_brake_implausibility(false); // Clear implausibility
            }
            if (fcu_status.get_brake_pedal_active() && calculated_torque > (MAX_TORQUE / 4)) {
                //fcu_status.set_brake_implausibility(true);
            }

            if (fcu_status.get_brake_implausibility() || fcu_status.get_accelerator_implausibility()) {
                // Implausibility exists, command 0 torque
                calculated_torque = 0;
                rampRatio = 0;
            }

            // FSAE FMEA specifications
            if (!rcu_status.get_bms_ok_high()) {
                calculated_torque = 0;
            }
            
            if (!rcu_status.get_imd_okhs_high()) {
                calculated_torque = 0;
            }
            
            if (debug && timer_debug_torque.check()) {
                Serial.print("FCU REQUESTED TORQUE: ");
                Serial.println(calculated_torque);
                Serial.print("FCU IMPLAUS THROTTLE: ");
                Serial.println(fcu_status.get_accelerator_implausibility());
                Serial.print("FCU IMPLAUS BRAKE: ");
                Serial.println(fcu_status.get_brake_implausibility());
            }
            
            mc_command_message.set_torque_command(calculated_torque);

            mc_command_message.write(msg.buf);
            msg.id = ID_MC_COMMAND_MESSAGE;
            msg.len = 8;
            CAN.write(msg);
        }
        break;
    }

    /*
     * Send a message to the Motor Controller over CAN when vehicle is not ready to drive
     */
    if (fcu_status.get_state() < FCU_STATE_READY_TO_DRIVE && timer_motor_controller_send.check()) {
        MC_command_message mc_command_message = MC_command_message(0, 0, 0, 0, 0, 0);
        if (fcu_status.get_state() >= FCU_STATE_ENABLING_INVERTER) {
            mc_command_message.set_inverter_enable(true);
        }
        mc_command_message.write(msg.buf);
        msg.id = ID_MC_COMMAND_MESSAGE;
        msg.len = 8;
        CAN.write(msg);
    }

    /*
     * Blink start led
     */
    if ((led_start_type == 2 && timer_led_start_blink_fast.check()) || (led_start_type == 3 && timer_led_start_blink_slow.check())) {
        if (led_start_active) {
            digitalWrite(LED_START, LOW);
        } else {
            digitalWrite(LED_START, HIGH);
        }
        led_start_active = !led_start_active;
    }

    /*
     * Handle start button press and depress
     */
    if (digitalRead(BTN_START) == btn_start_pressed && !btn_start_debouncing) { // Value is different than stored
        btn_start_debouncing = true;
        timer_btn_start.reset();
    }
    if (btn_start_debouncing && digitalRead(BTN_START) != btn_start_pressed) { // Value returns during debounce period
        btn_start_debouncing = false;
    }
    if (btn_start_debouncing && timer_btn_start.check()) { // Debounce period finishes without value returning
        btn_start_pressed = !btn_start_pressed;
        if (btn_start_pressed) {
            fcu_status.set_start_button_press_id(fcu_status.get_start_button_press_id() + 1);
            Serial.print("FCU START BUTTON ID: ");
            Serial.println(fcu_status.get_start_button_press_id());
        }
    }
}

/*
 * Read values of sensors
 */
void read_values() {
    fcu_readings.set_accelerator_pedal_raw_1(read_adc(ADC_ACCEL_1_CHANNEL));
    fcu_readings.set_accelerator_pedal_raw_2(read_adc(ADC_ACCEL_2_CHANNEL));
    fcu_readings.set_brake_pedal_raw(read_adc(ADC_BRAKE_CHANNEL));
    if (fcu_readings.get_brake_pedal_raw() >= BRAKE_ACTIVE) {
        fcu_status.set_brake_pedal_active(true);
    } else {
        fcu_status.set_brake_pedal_active(false);
    }
    if (debug && timer_debug.check()) {
        Serial.print("FCU PEDAL THROTTLE 1: ");
        Serial.println(fcu_readings.get_accelerator_pedal_raw_1());
        Serial.print("FCU PEDAL THROTTLE 2: ");
        Serial.println(fcu_readings.get_accelerator_pedal_raw_2());
        Serial.print("FCU PEDAL BRAKE: ");
        Serial.println(fcu_readings.get_brake_pedal_raw());
        Serial.print("FCU BRAKE ACT: ");
        Serial.println(fcu_status.get_brake_pedal_active());
        Serial.print("FCU STATE: ");
        Serial.println(fcu_status.get_state());
    }
    // TODO calculate temperature
}

/*
 * Set the Start LED
 */
void set_start_led(uint8_t type) {
    if (led_start_type != type) {
        led_start_type = type;

        if (type == 0) {
            digitalWrite(LED_START, LOW);
            led_start_active = false;
            Serial.println("FCU Setting Start LED off");
            return;
        }

        digitalWrite(LED_START, HIGH);
        led_start_active = true;

        if (type == 1) {
            Serial.println("FCU Setting Start LED solid on");
        } else if (type == 2) {
            timer_led_start_blink_fast.reset();
            Serial.println("FCU Setting Start LED fast blink");
        } else if (type == 3) {
            timer_led_start_blink_slow.reset();
            Serial.println("FCU Setting Start LED slow blink");
        }
    }
}

/*
 * Handle changes in state
 */
void set_state(uint8_t new_state) {
    if (fcu_status.get_state() == new_state) {
        return;
    }
    fcu_status.set_state(new_state);
    if (new_state == FCU_STATE_TRACTIVE_SYSTEM_NOT_ACTIVE) {
        set_start_led(0);
    }
    if (new_state == FCU_STATE_TRACTIVE_SYSTEM_ACTIVE) {
        set_start_led(2);
        btn_start_new = fcu_status.get_start_button_press_id() + 1;
    }
    if (new_state == FCU_STATE_ENABLING_INVERTER) {
        set_start_led(1);
        Serial.println("FCU Enabling inverter");
        MC_command_message mc_command_message = MC_command_message(0, 0, 0, 1, 0, 0);
        msg.id = 0xC0;
        msg.len = 8;
        for(int i = 0; i < 10; i++) {
            mc_command_message.write(msg.buf); // many enable commands
            CAN.write(msg);
        }
        mc_command_message.set_inverter_enable(false);
        mc_command_message.write(msg.buf); // disable command
        CAN.write(msg);
        for(int i = 0; i < 10; i++) {
            mc_command_message.set_inverter_enable(true);
            mc_command_message.write(msg.buf); // many more enable commands
            CAN.write(msg);
        }
        Serial.println("FCU Sent enable command");
        timer_inverter_enable.reset();
    }
    if (new_state == FCU_STATE_WAITING_READY_TO_DRIVE_SOUND) {
        timer_ready_sound.reset();
        digitalWrite(READY_SOUND, HIGH);
        Serial.println("Inverter enabled");
        Serial.println("RTDS enabled");
    }
    if (new_state == FCU_STATE_READY_TO_DRIVE) {
        digitalWrite(READY_SOUND, LOW);
        Serial.println("RTDS deactivated");
        Serial.println("Ready to drive");
    }
}

/*
 * Read analog value from MCP3208 ADC
 * Credit to Arduino Playground
 * https://playground.arduino.cc/Code/MCP3208
 */
int read_adc(int channel) {
    int adcvalue = 0;
    byte commandbits = B11000000; // Command bits - start, mode, chn (3), dont care (3)
    commandbits|=((channel)<<3); // Select channel
    digitalWrite(ADC_SPI_CS, LOW); // Select adc
    for (int i=7; i>=3; i--){ // Write bits to adc
        digitalWrite(ADC_SPI_DOUT, commandbits&1<<i);
        digitalWrite(ADC_SPI_SCK, HIGH); // Cycle clock
        digitalWrite(ADC_SPI_SCK, LOW);    
    }
    digitalWrite(ADC_SPI_SCK, HIGH);  // Ignore 2 null bits
    digitalWrite(ADC_SPI_SCK, LOW);
    digitalWrite(ADC_SPI_SCK, HIGH);  
    digitalWrite(ADC_SPI_SCK, LOW);
    for (int i=11; i>=0; i--){ // Read bits from adc
        adcvalue+=digitalRead(ADC_SPI_DIN)<<i;
        digitalWrite(ADC_SPI_SCK, HIGH); // Cycle clock
        digitalWrite(ADC_SPI_SCK, LOW);
    }
    digitalWrite(ADC_SPI_CS, HIGH); // Turn off device
    return adcvalue;
}
