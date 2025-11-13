#include <Wire.h>
#include "Adafruit_AS7343.h"

Adafruit_AS7343 as7343;

void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);

  if (!as7343.begin()) {
    Serial.println("AS7343 NOT found!");
    while(1);
  }
  Serial.println("AS7343 connected.");
}

void loop() {
  uint16_t readings[12];

  as7343.readAllChannels(readings);
  for (int i=0; i<12; i++){
    Serial.print(readings[i]);
    Serial.print("\t");
  }
  Serial.println();
  delay(500);
}
