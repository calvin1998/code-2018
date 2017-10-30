#include <HyTech17.h>
#include <Metro.h>

/*
 * Pin definitions
 */
#define COOL_MOSFET_1 A8
#define COOL_MOSFET_2 A7
#define COOL_MOSFET_3 A6
#define COOL_RELAY_1 A9
#define COOL_RELAY_2 2
#define SENSE_BMS A1
#define SENSE_IMD A0
#define SENSE_SHUTDOWN_OUT A2
#define SSR_BRAKE_LIGHT 12
#define SSR_INVERTER 6
#define SSR_LATCH_BMS 11
#define SSR_LATCH_IMD 8

/*
 * Constant definitions
 */
#define BMS_HIGH 100
#define BMS_LOW 50
#define IMD_HIGH 100
#define IMD_LOW 50

#define XB Serial2

unsigned long xbee_baud = 115200;
const int XBEE_LED = 13;
// byte xbee_buff[80];

void setup() {
    pinMode(COOL_RELAY_2, OUTPUT);
    pinMode(SSR_BRAKE_LIGHT, OUTPUT);
    pinMode(SSR_INVERTER, OUTPUT);
    pinMode(SSR_LATCH_BMS, OUTPUT);
    pinMode(SSR_LATCH_IMD, OUTPUT);
  
    Serial.begin(115200);
    // CAN.begin();
    XB.begin(xbee_baud);
    delay(100);
    Serial.println("CAN transceiver initialized");
    digitalWrite(SSR_INVERTER, HIGH);
    
    // Xbee setup
}

void loop() {
    int wr;
    wr = XB.availableForWrite();
    char buffer[3] = {'a', 'b', '\n'};
    Serial.println("looping");
    if (wr > 1) {
        Serial.println("sending value");
        // XB.write(buffer);
        XB.write(buffer, sizeof(buffer));

        digitalWrite(XBEE_LED, HIGH);
        delay(300);
        digitalWrite(XBEE_LED, LOW);
        delay(10);
    }
    delay(1000);
}