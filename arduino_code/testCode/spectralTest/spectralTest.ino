//#include <Adafruit_AS7343.h>
//
//Adafruit_AS7343 as7343;
//
//void setup() {
//  Serial.begin(115200);
//  while (!Serial) delay(10);
//
//  Serial.println("AS7343 Spectral Sensor Test");
//
//  // Initialize I2C with your WORKING pin numbers
//  // Note: Update these to match whatever setup just worked for you!
//  Wire.begin(1, 2); 
//
//  if (!as7343.begin()) {
//    Serial.println("Could not find AS7343. Check soldering!");
//    while (1) delay(10);
//  }
//
//  // --- NEW CODE TO TURN ON THE LIGHT ---
//  Serial.println("Turning on LED...");
//  
//  // 1. Set the brightness (4mA to 100mA)
//  as7343.setLEDCurrent(20); // Start with 20mA (Medium Brightness)
//  
//  // 2. Turn it ON
//  as7343.enableLED(true);
//
//  Serial.println("AS7343 Found!");
//  
//  // Set generic gain/timing settings
//  as7343.setATIME(100);
//  as7343.setASTEP(999);
//  as7343.setGain(AS7343_GAIN_64X);
//}
//
//void loop() {
//  // Read all channels
//  uint16_t readings[12];
//  if (as7343.readAllChannels(readings)) {
//    Serial.print("Violet: "); Serial.print(readings[0]);
//    Serial.print("  Blue: "); Serial.print(readings[2]);
//    Serial.print("  Green: "); Serial.print(readings[5]);
//    Serial.print("  Orange: "); Serial.print(readings[7]);
//    Serial.print("  Red: "); Serial.print(readings[10]);
//    Serial.println();
//  } else {
//    Serial.println("Error reading channels");
//  }
//  delay(1000);
//}

//#include <Adafruit_AS7343.h>
//
//Adafruit_AS7343 as7343;
//
//void setup() {
//  Serial.begin(115200);
//  while (!Serial) delay(10);
//  
//  Wire.begin(1, 2); // Your verified pins
//
//  if (!as7343.begin()) {
//    Serial.println("Sensor NOT found (Check Wiring)");
//    while(1) delay(10);
//  }
//  
//  Serial.println("Sensor Found! Configuring...");
//  as7343.setATIME(100);
//  as7343.setGain(AS7343_GAIN_256X); // High sensitivity
//  
//  // Turn on LED for test
//  as7343.setLEDCurrent(0);
//  as7343.enableLED(false);
//}
//
//void loop() {
//  uint16_t readings[12];
//  
//  // The CRITICAL check: Did the read succeed?
//  bool success = as7343.readAllChannels(readings);
//  
//  if (success) {
//    Serial.print("Data Valid!  Orange: "); 
//    Serial.println(readings[7]); // Print just one color
//  } else {
//    Serial.println("READ ERROR: Connection unstable (Solder joints?)");
//  }
//  
//  delay(500);
//}

//#include <Arduino.h>
//#include <Wire.h>
//
//#define AS7343_ADDR 0x39 
//
//// --- PINS (Use your verified setup) ---
//#define SDA_PIN 1
//#define SCL_PIN 2
//
//void setup() {
//  Serial.begin(115200);
//  while (!Serial) delay(10);
//  Serial.println("\n--- AS7343 MANUAL MODE (ID BYPASS) ---");
//
//  Wire.begin(SDA_PIN, SCL_PIN);
//
//  // 1. FORCE POWER ON (Write 0x0B to Register 0x80)
//  Wire.beginTransmission(AS7343_ADDR);
//  Wire.write(0x80); // ENABLE Register
//  Wire.write(0x0B); // Power ON + Spectral ON
//  Wire.endTransmission();
//  
//  delay(100); // Wait for boot
//
//  // 2. SET GAIN TO MAX (So we can see indoor light)
//  // Write 0xAA (Gain 1024x) to Register 0xAA (CONTROL)
//  Wire.beginTransmission(AS7343_ADDR);
//  Wire.write(0xAA); 
//  Wire.write(0x10); // Gain 256x (Try 0x1D for 2048x if still dark)
//  Wire.endTransmission();
//
//  // 3. SET INTEGRATION TIME (Exposure)
//  Wire.beginTransmission(AS7343_ADDR);
//  Wire.write(0x81); // ATIME
//  Wire.write(0xC0); // ~180ms
//  Wire.endTransmission();
//
//  Serial.println("Sensor Configured. Starting Stream...");
//}
//
//void loop() {
//  // We will read the "Orange" Channel (Data Reg 0x9C)
//  // Low Byte: 0x9C, High Byte: 0x9D
//  
//  Wire.beginTransmission(AS7343_ADDR);
//  Wire.write(0x9C); // Pointer to Orange Low Byte
//  Wire.endTransmission();
//  
//  Wire.requestFrom(AS7343_ADDR, 2); // Ask for 2 bytes
//  
//  if (Wire.available() == 2) {
//    byte low = Wire.read();
//    byte high = Wire.read();
//    uint16_t orange = (high << 8) | low;
//    
//    Serial.print("Orange Channel: "); 
//    Serial.print(orange);
//    
//    if (orange < 50) Serial.println(" (Dark/Background)");
//    else if (orange > 1000) Serial.println(" (Bright! IT WORKS!)");
//    else Serial.println(" (Dim Light)");
//  } else {
//    Serial.println("Read Failed.");
//  }
//  
//  delay(200);
//}

//
//#include <Arduino.h>
//#include <Wire.h>
//
//#define AS7343_ADDR 0x39 // Standard address for SparkFun/Adafruit boards
//
//// --- YOUR VERIFIED PINS ---
//#define SDA_PIN 1
//#define SCL_PIN 2
//
//void setup() {
//  Serial.begin(115200);
//  while (!Serial) delay(10);
//  Serial.println("\n--- AS7343 DEEP DIAGNOSTIC ---");
//
//  Wire.begin(SDA_PIN, SCL_PIN);
//
//  // 1. CHECK CONNECTION
//  Wire.beginTransmission(AS7343_ADDR);
//  if (Wire.endTransmission() != 0) {
//    Serial.println("CRITICAL FAILURE: Sensor not found at 0x39.");
//    Serial.println(" -> Check Wiring (SDA/SCL swapped?)");
//    Serial.println(" -> Check Power (3V3 connected?)");
//    while(1);
//  }
//  Serial.println(" [OK] I2C Connection Alive.");
//
//  // 2. READ CHIP ID (Who am I?)
//  // Register 0x92 is the ID register
//  writeReg(0x92); 
//  byte chipID = readReg();
//  
//  Serial.print(" [ID] Chip ID Read: 0x"); Serial.println(chipID, HEX);
//  
//  if (chipID == 0x24) {
//    Serial.println("      -> MATCH! This is an AS7343.");
//  } else if (chipID == 0x09) {
//    Serial.println("      -> WARNING: This is an AS7341! You need the AS7341 Library.");
//  } else {
//    Serial.println("      -> ERROR: Unknown Chip ID. Possible power issue?");
//  }
//
//  // 3. FORCE ENABLE (Open the Eye)
//  // Register 0x80 is the ENABLE register. 
//  // We write 0x0B (Power ON + Spectral Enable + Wait Enable)
//  Serial.println(" [CMD] Forcing Power ON...");
//  writeRegData(0x80, 0x0B); 
//  delay(100); // Wait for boot
//
//  // 4. READ STATUS
//  writeReg(0x80);
//  byte status = readReg();
//  Serial.print(" [STS] Status Register: 0x"); Serial.println(status, HEX);
//  if (status & 0x01) Serial.println("      -> Power is ON."); 
//  else Serial.println("      -> Power is OFF (Hardware Failure).");
//
//  // 5. READ RAW DATA (Orange Channel)
//  // Orange Low Byte is 0x9C
//  Serial.print(" [DAT] Reading Raw Data... ");
//  writeReg(0x9C);
//  byte low = readReg();
//  writeReg(0x9D);
//  byte high = readReg();
//  uint16_t rawVal = (high << 8) | low;
//  
//  Serial.print("Value: "); Serial.println(rawVal);
//}
//
//void loop() {}
//
//// --- HELPER FUNCTIONS ---
//void writeReg(byte reg) {
//  Wire.beginTransmission(AS7343_ADDR);
//  Wire.write(reg);
//  Wire.endTransmission();
//}
//
//void writeRegData(byte reg, byte val) {
//  Wire.beginTransmission(AS7343_ADDR);
//  Wire.write(reg);
//  Wire.write(val);
//  Wire.endTransmission();
//}
//
//byte readReg() {
//  Wire.requestFrom(AS7343_ADDR, 1);
//  if (Wire.available()) return Wire.read();
//  return 0xFF; // Error code
//}


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
