#include <ESP32Servo.h>

Servo myservo;

void setup() {
  myservo.attach(13);
}

void loop() {
  myservo.write(0);   // move to 0°
  delay(1000);
  myservo.write(90);  // move to 90°
  delay(1000);
  myservo.write(180); // move to 180°
  delay(1000);
}
