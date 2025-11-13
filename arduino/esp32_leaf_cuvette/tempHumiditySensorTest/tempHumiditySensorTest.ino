
//testing if board is found through i2s

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
}

void loop() {
  Serial.println("Scanning...");
  Wire.beginTransmission(0x44);
  if (Wire.endTransmission() == 0) {
    Serial.println("SHT31 FOUND at 0x44!");
  } else {
    Serial.println("Not found");
  }
  delay(1000);
}


//testing humidity and temperature

#include <Wire.h>
#include "Adafruit_SHT31.h"

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);

  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 not found!");
    while(1);
  }
}

void loop() {
  Serial.print("Temp °C = ");
  Serial.print(sht31.readTemperature());
  Serial.print("\tRH % = ");
  Serial.println(sht31.readHumidity());
  delay(1000);
}
