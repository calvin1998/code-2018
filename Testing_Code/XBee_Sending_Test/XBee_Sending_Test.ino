#include <HyTech17.h>

/*
 * Constant definitions
 */
#define XB Serial2

unsigned long xbee_baud = 115200;
const int XBEE_LED = 13;

void setup() {
  
    Serial.begin(115200);

    XB.begin(xbee_baud);
    delay(100);
    Serial.println("XBee transceiver initialized");
    
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