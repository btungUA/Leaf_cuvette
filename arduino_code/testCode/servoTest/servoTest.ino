#include <ESP32Servo.h>

Servo myservo;

int currAngle = 0;

void setup() {
  myservo.attach(13);
}

void loop() {
  //rotateServo(45);
  myservo.write(0);
  delay(1000);
  myservo.write(45);
  delay(1000);
  myservo.write(90);
  delay(1000);
  myservo.write(135);
  delay(1000);
  myservo.write(180);
  delay(1000);
}
