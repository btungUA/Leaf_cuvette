#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>

// --- NETWORK CONFIGURATION ---
const char* WIFI_SSID = "Leaf-Link";
const char* WIFI_PASS = "cuvettemaster";
const char* MQTT_SERVER = "10.42.0.1"; // The Pi's IP
const int MQTT_PORT = 1883;

// --- PIN DEFINITIONS ---
#define SDA_PIN 1
#define SCL_PIN 2
#define THERMISTOR_PIN 4      // ADC1 on S3 (Safe)
#define MOSFET_PIN 13

// --- SPECTRAL SENSOR SETTINGS ---
#define SPECTRAL_ADDR 0x39 

// --- OBJECT INSTANTIATION ---
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// --- CONSTANTS (Thermistor) ---
const float VCC = 3.30;
const float SERIES_R = 10000.0;
const float R0 = 10000.0;
const float T0 = 25.0 + 273.15;
const float BETA = 3950.0;

// --- VARIABLES FOR MULTITASKING & AVERAGING ---
unsigned long lastMsgTime = 0;
const long interval = 5000; // Publish every 5 seconds
int sampleCount = 0;

float sumShtTemp = 0;
float sumShtHum = 0;
float sumThermTemp = 0;
long sumSpecVio = 0;
long sumSpecGrn = 0;
long sumSpecRed = 0;

// --- VARIABLES FOR MOSFET TIMING ---
unsigned long previousMosfetTime = 0;
const long mosfetOnDuration = 780000;  // 13 minutes in ms
const long mosfetOffDuration = 120000; // 2 minutes in ms
bool mosfetIsOn = false;

// --- HELPER: SPECTRAL SENSOR ---
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

void getSpectralData(uint16_t &vio, uint16_t &grn, uint16_t &red) {
  Wire.beginTransmission(SPECTRAL_ADDR);
  Wire.write(0x95); 
  Wire.endTransmission();
  
  Wire.requestFrom((int)SPECTRAL_ADDR, 12);
  if (Wire.available() == 12) {
    vio = readSpectral16(); // 415nm
    readSpectral16(); // Skip blu
    readSpectral16(); // Skip grn (480nm)
    readSpectral16(); // Skip yel
    grn = readSpectral16(); // org (555nm) mapped to grn
    red = readSpectral16(); // red (590nm)
  } else {
    vio = 0; grn = 0; red = 0;
  }
}

// --- HELPER: THERMISTOR ---
float getThermistorTemp() {
  int adcVal = analogRead(THERMISTOR_PIN);
  if (adcVal <= 0) return 0.0;
  float voltage = adcVal * (VCC / 4095.0);
  float Rtherm = (SERIES_R * (VCC / voltage - 1.0));
  float steinhart = log(Rtherm / R0) / BETA + 1.0 / T0;
  return (1.0 / steinhart) - 273.15;
}

// --- MQTT CALLBACK (Receiving Commands) ---
void callback(char* topic, byte* message, unsigned int length) {
  // Empty for now, as MOSFET is running autonomously on a timer
}

// --- NETWORK SETUP ---
void setup_wifi() {
  delay(10);
  Serial.print("Connecting to WiFi: "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32LeafClient")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// --- MAIN SETUP ---
void setup() {
  Serial.begin(115200);
  
  // 1. I2C & Pins
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(THERMISTOR_PIN, INPUT);

  // Initialize MOSFET Pin and start the cycle ON
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);
  mosfetIsOn = true;
  previousMosfetTime = millis();
  Serial.println("MOSFET: Initialized ON (Starting 13 min cycle)");

  // 2. Hardware Init
  sht31.begin(0x44); 
  writeSpectralReg(0x80, 0x0B); delay(100); 
  writeSpectralReg(0xAA, 0x04); 
  writeSpectralReg(0x81, 0xFF); 
  writeSpectralReg(0x82, 0xFF); 

  // 3. Network Init
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

// --- MAIN LOOP ---
void loop() {
  if (!client.connected()) reconnect();
  client.loop(); 

  unsigned long currentMillis = millis();

  // --- MOSFET TIMER LOGIC ---
  if (mosfetIsOn) {
    if (currentMillis - previousMosfetTime >= mosfetOnDuration) {
      digitalWrite(MOSFET_PIN, LOW); // Turn OFF
      mosfetIsOn = false;
      previousMosfetTime = currentMillis;
      Serial.println("MOSFET: Switched OFF (Starting 2 min cycle)");
    }
  } else {
    if (currentMillis - previousMosfetTime >= mosfetOffDuration) {
      digitalWrite(MOSFET_PIN, HIGH); // Turn ON
      mosfetIsOn = true;
      previousMosfetTime = currentMillis;
      Serial.println("MOSFET: Switched ON (Starting 13 min cycle)");
    }
  }

  // --- FAST LOOP (Sample every 100ms) ---
  static unsigned long lastSampleTime = 0;
  if (currentMillis - lastSampleTime > 100) {
    lastSampleTime = currentMillis;
    
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t)) sumShtTemp += t;
    if (!isnan(h)) sumShtHum += h;
    
    sumThermTemp += getThermistorTemp();
    
    uint16_t v, g, r;
    getSpectralData(v, g, r);
    sumSpecVio += v;
    sumSpecGrn += g;
    sumSpecRed += r;
    
    sampleCount++;
  }

  // --- SLOW LOOP (Publish every 5 seconds) ---
  if (currentMillis - lastMsgTime > interval) {
    lastMsgTime = currentMillis;

    if (sampleCount > 0) {
      StaticJsonDocument<512> doc;
      doc["sensor"] = "leaf_node_1";
      doc["temp_air"] = sumShtTemp / sampleCount;
      doc["humidity"] = sumShtHum / sampleCount;
      doc["temp_leaf"] = sumThermTemp / sampleCount;
      
      doc["spectral_vio"] = sumSpecVio / sampleCount;
      doc["spectral_grn"] = sumSpecGrn / sampleCount;
      doc["spectral_red"] = sumSpecRed / sampleCount;
      
      doc["mosfet_state"] = mosfetIsOn ? 1 : 0; // 1 if ON, 0 if OFF

      char buffer[512];
      serializeJson(doc, buffer);
      
      client.publish("sensors/leaf_1", buffer);
      
      Serial.println("Published: " + String(buffer));
      Serial.print("AirTemp: "); Serial.println(sumShtTemp / sampleCount);
      Serial.print("AirHum: ");  Serial.println(sumShtHum / sampleCount);
      Serial.println();

      // Reset accumulators
      sumShtTemp = 0; sumShtHum = 0; sumThermTemp = 0;
      sumSpecVio = 0; sumSpecGrn = 0; sumSpecRed = 0;
      sampleCount = 0;
    }
  }
}