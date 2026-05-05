#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SHT31.h>
#include <ESP32Servo.h>

// --- PIN DEFINITIONS ---
#define SDA_PIN 1
#define SCL_PIN 2
#define THERMISTOR_PIN 4      // ADC1 on S3 (Safe)
#define OPTICAL_SENSOR_PIN 5  // ADC1 on S3 (Safe)
#define SERVO_PIN 13

unsigned long myTime;

// --- SPECTRAL SENSOR MANUAL SETTINGS ---
#define SPECTRAL_ADDR 0x39 // Standard Address for AS7341/43

// --- OBJECT INSTANTIATION ---
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Servo myServo;

// --- CONSTANTS ---
const float VCC = 3.30;
const float SERIES_R = 10000.0;
const float R0 = 10000.0;
const float T0 = 25.0 + 273.15;
const float BETA = 3950.0;

// --- HELPER FUNCTIONS FOR MANUAL SPECTRAL READ ---
void writeSpectralReg(byte reg, byte val) {
  Wire.beginTransmission(SPECTRAL_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint16_t readSpectral16() {
  byte low = Wire.read();
  byte high = Wire.read();
  return (high << 8) | low;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\n--- Leaf Cuvette Full Hardware Test ---");

  // 1. Initialize I2C (Shared Bus)
  Wire.begin(SDA_PIN, SCL_PIN);

  // 2. I2C Scanner (Verification)
  Serial.print("Scanning I2C Bus... ");
  int devices = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("0x"); Serial.print(address, HEX); Serial.print(" ");
      devices++;
    }
  }
  if (devices == 0) Serial.println("No devices found (Check SDA/SCL!)");
  else Serial.println("Done.");
  
  // 3. Initialize SHT31 (Temp/Hum)
  // Try 0x44 first, then 0x45
  if (!sht31.begin(0x44)) { 
     if (!sht31.begin(0x45)) {
        Serial.println("[-] SHT31 NOT found");
     } else {
        Serial.println("[+] SHT31 Found at 0x45");
     }
  } else {
    Serial.println("[+] SHT31 Found at 0x44");
  }

  // 4. Initialize SPECTRAL SENSOR (Manual Override Mode)
  Serial.print("Initializing Spectral Sensor... ");
  // A. Wake Up (Power ON + Spectral Engine ON)
  writeSpectralReg(0x80, 0x0B); 
  delay(100);
  // B. Set Gain (High Sensitivity: 16x)
  writeSpectralReg(0xAA, 0x04); 
  // C. Set Exposure (Fastest) to see indoor light
  writeSpectralReg(0x81, 0xFF); 
  writeSpectralReg(0x82, 0xFF);
  Serial.println("Configured (Manual Mode).");

  // 5. Initialize Servo
//  myServo.setPeriodHertz(50); 
//  myServo.attach(SERVO_PIN, 500, 2400); 
//  Serial.println("[+] Servo Attached.");

  // 6. Pin Modes
  pinMode(THERMISTOR_PIN, INPUT);
  pinMode(OPTICAL_SENSOR_PIN, INPUT);
}

void loop() {
  Serial.println("\n--- SENSOR READINGS ---");
  myTime = millis() / 1000;
  Serial.print("Time since start "); Serial.print(myTime); Serial.println("s");

  // --- SHT31 ---
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  if (!isnan(t)) {
    Serial.print("SHT31 Temp: "); Serial.print(t); Serial.print(" C | Hum: "); Serial.print(h); Serial.println(" %");
  } else {
    Serial.println("SHT31 Read Error");
  }

  // --- THERMISTOR (Steinhart-Hart) ---
  int adcVal = analogRead(THERMISTOR_PIN);
  if (adcVal > 0) {
     float voltage = adcVal * (VCC / 4095.0);
     float Rtherm = (SERIES_R * (VCC / voltage - 1.0));
     float steinhart = log(Rtherm / R0) / BETA + 1.0 / T0;
     steinhart = 1.0 / steinhart;
     float tempC = steinhart - 273.15;
     Serial.print("Thermistor: "); Serial.print(tempC); Serial.print(" C (Raw: "); Serial.print(adcVal); Serial.println(")");
  } else {
     Serial.println("Thermistor Error (Raw: 0)");
  }

  // --- OPTICAL ---
  int optVal = analogRead(OPTICAL_SENSOR_PIN);
  Serial.print("Optical: "); Serial.print(optVal);
  if (optVal > 2500) Serial.println(" (Clear/Empty)"); else Serial.println(" (Blocked/Leaf)");

  // --- SPECTRAL SENSOR (Manual Read) ---
  // Request 12 Bytes starting from Register 0x95
  Wire.beginTransmission(SPECTRAL_ADDR);
  Wire.write(0x95); 
  Wire.endTransmission();
  
  Wire.requestFrom(SPECTRAL_ADDR, 12);
  
  if (Wire.available() == 12) {
    uint16_t vio = readSpectral16(); // 415nm
    uint16_t blu = readSpectral16(); // 445nm
    uint16_t grn = readSpectral16(); // 480nm
    uint16_t yel = readSpectral16(); // 515nm
    uint16_t org = readSpectral16(); // 555nm (Green-Yellow)
    uint16_t red = readSpectral16(); // 590nm (Orange-Red)
    
    Serial.print("Spectral: [Vio:"); Serial.print(vio);
    Serial.print(" Grn:"); Serial.print(org); // 555nm is key for chlorophyll
    Serial.print(" Red:"); Serial.print(red);
    Serial.println("]");
  } else {
    Serial.println("Spectral Read Error");
  }

  // --- SERVO TEST ---
//  Serial.println("Actuating Servo...");
//  myServo.write(45); // Open slightly
//  delay(500);
//  myServo.write(90); // Close
//  delay(500);
  
  delay(1000); 
}
