#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// --- PIN DEFINITIONS (ESP32-S3) ---
#define SDA_PIN 1
#define SCL_PIN 2

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for Serial Monitor

  Serial.println("\n--- SHT31 Sensor Test ---");

  // Initialize I2C with specific pins
  Wire.begin(SDA_PIN, SCL_PIN);

  // Try to initialize at default address (0x44)
  if (!sht31.begin(0x44)) {
    // If that fails, try the alternative address (0x45)
    if (!sht31.begin(0x45)) {
      Serial.println("[-] Failed to find SHT31 sensor. Check wiring & soldering.");
      while (1) delay(10); // Halt if not found
    } else {
      Serial.println("[+] SHT31 Found at address 0x45.");
    }
  } else {
    Serial.println("[+] SHT31 Found at address 0x44.");
  }
}

void loop() {
  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();

  if (!isnan(temp)) {
    Serial.print("Temp: "); 
    Serial.print(temp); 
    Serial.print(" °C");
    
    Serial.print("\t Humidity: "); 
    Serial.print(hum);
    Serial.println(" %");
  } else {
    Serial.println("Failed to read temperature/humidity");
  }

  delay(1000); // Read every 1 second
}
