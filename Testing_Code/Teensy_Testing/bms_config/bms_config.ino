#include <FlexCAN.h>
#include <Metro.h>
#include <HyTech17.h>

FlexCAN CAN(500000);
CAN_message_t msg;
String command_incoming;
bool command_finished = false;

void setup() {
  Serial.begin(115200); //initialize serial for PC communication
  CAN.begin();
  delay(200);
  Serial.println("CAN transceiver initialized");
  Serial.println("Enter <address>,<read=0|write=1>,<value>");
}

uint32_t t = 1;

void loop() {
  while (CAN.read(msg)) {
    if (msg.id == ID_BMS_CONFIG) {
      BMS_config bms_config = BMS_config(msg.buf);
      if (bms_config.isWrite()) {
        Serial.print("Updated address ");
        Serial.print(bms_config.getAddress());
        Serial.print(" to value ");
        Serial.println(bms_config.getValue());
      } else {
        Serial.print("Address ");
        Serial.print(bms_config.getAddress());
        Serial.print(": ");
        Serial.println(bms_config.getValue());
      }
    }
  }

  /*
   * Get user input
   */
  if (Serial.available() > 0) {
    char newChar = (char) Serial.read();
    if (newChar != '\n') {
      command_incoming += newChar;
    } else {
      command_finished = true;
    }
  }

  if (command_finished) {
    command_finished = false;
    char *params[3]; // address,value
    char *cmd = const_cast<char*> (command_incoming.c_str());
    params[0] = strtok(cmd, ",");
    int i = 0;
    while (params[i] != NULL) {
        params[++i] = strtok(NULL, ",");
    }

    uint8_t address = atoi(params[0]);
    uint8_t writeFlag = atoi(params[1]);
    short value = atoi(params[2]);
    BMS_config bms_config = BMS_config();
    bms_config.setAddress(address);
    bms_config.setValue(value);
    bms_config.setWriteFlag(writeFlag);
    bms_config.write(msg.buf);
    msg.id = ID_BMS_CONFIG;
    msg.len = 4;
    CAN.write(msg);
    Serial.println("Sent to BMS");
    command_incoming = "";
  }
}
