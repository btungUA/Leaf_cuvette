int thermPin = 34;

void setup() {
  Serial.begin(115200);
}

void loop() {
  int reading = analogRead(thermPin);
  Serial.println(reading);
  delay(200);
}
