
#define TEMP_PIN A1     //temperature reading pin
#define THERMOCOUPLE(x) (21.71 * (x) - 44.77)
double swag;
void setup() {
    Serial.begin(9600);   //9600 bits per second
    analogReference(INTERNAL2V56);     //nominal 2.56 V reference.
    pinMode(TEMP_PIN, INPUT);
}

void loop() {
    swag = analogRead(TEMP_PIN);
    //swag = swag / 1024 * 2.56 * 3.3;
    //swag = THERMOCOUPLE(swag);
    Serial.println(swag);
    delay(1000);
}
