void setup() {
    // put your setup code here, to run once:
    Serial.begin(9600);
    delay(100);
}

void loop() {
    // put your main code here, to run repeatedly:
    for(int i = 0; i < 100; i++) {
        Serial.println(i);
        delay(100);
    }

}