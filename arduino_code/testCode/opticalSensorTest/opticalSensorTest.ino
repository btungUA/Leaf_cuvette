int sensorPin = 5; // ADC channel

void setup() {
  Serial.begin(115200);
}

void loop() {
  int value = analogRead(sensorPin);
  Serial.println(value);
  delay(200);
}
