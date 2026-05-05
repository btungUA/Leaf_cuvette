#include <Arduino.h>
#include <Wire.h>

// --- PIN DEFINITIONS ---
#define SDA_PIN 1
#define SCL_PIN 2
#define AS7341_ADDR 0x39

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("\n--- SPECTRAL SENSOR MANUAL OVERRIDE ---");

  Wire.begin(SDA_PIN, SCL_PIN);

  // 1. WAKE UP (Power ON + Spectral ON)
  writeRegister(0x80, 0x0B); 
  delay(100);

  // 2. CONFIGURE GAIN (High Sensitivity - 256x)
  // Register 0xAA = Control. Write 0x10 for 256x gain.
  writeRegister(0xAA, 0x10);

  // 3. CONFIGURE LED (Turn on the Flashlight!)
  // Bank selection needed for LED usually, but let's try the direct pin drive
  // (Some boards link LED to the INT pin or LDR pin. We will try default ENABLE)
  // Just in case, let's max out the exposure so it sees ROOM light
  writeRegister(0x81, 0xFF); // ATIME (Fastest)
  writeRegister(0x82, 0xFF); // ASTEP (Fastest)
  
  Serial.println("Sensor Configured. Shine a light on it!");
}

void loop() {
  // 4. READ ALL CHANNELS
  // The AS7341 stores data starting at register 0x95 (A_Low) up to 0xA6 (L_High)
  // We will read 12 bytes of data
  
  Wire.beginTransmission(AS7341_ADDR);
  Wire.write(0x95); // Start reading at Channel 0
  Wire.endTransmission();
  
  Wire.requestFrom(AS7341_ADDR, 12); // Request 12 bytes
  
  if (Wire.available() == 12) {
    uint16_t ch0 = read16(); // F1 (415nm - Violet)
    uint16_t ch1 = read16(); // F2 (445nm - Blue)
    uint16_t ch2 = read16(); // F3 (480nm)
    uint16_t ch3 = read16(); // F4 (515nm - Cyan)
    uint16_t ch4 = read16(); // F5 (555nm - Green)
    uint16_t ch5 = read16(); // F6 (590nm - Yellow/Orange)
    
    Serial.print("VIOLET: "); Serial.print(ch0);
    Serial.print(" | GREEN: "); Serial.print(ch4);
    Serial.print(" | ORANGE: "); Serial.print(ch5);
    
    // THE ULTIMATE TEST:
    if (ch5 > 1000) Serial.println("  <-- LIGHT DETECTED! (IT WORKS)");
    else Serial.println("  (Low Light)");
    
  } else {
    Serial.println("Read Failed (Check Solder Connections)");
  }
  
  delay(500);
}

// --- HELPERS ---
void writeRegister(byte reg, byte val) {
  Wire.beginTransmission(AS7341_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint16_t read16() {
  byte low = Wire.read();
  byte high = Wire.read();
  return (high << 8) | low;
}
