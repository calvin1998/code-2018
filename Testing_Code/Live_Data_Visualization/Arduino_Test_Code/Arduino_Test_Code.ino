int counter = 0;

void setup() {
    // put your setup code here, to run once:
    Serial.begin(9600);
    delay(100);
    counter = 0;
}

void loop() {
    // put your main code here, to run repeatedly:
    for(int i = 0; i < 100; i++) {
        // print to serial
        delay(100);
        Serial.print(counter++);
        Serial.print(" ");
        Serial.print(i);
        Serial.print("\n");
    }

}