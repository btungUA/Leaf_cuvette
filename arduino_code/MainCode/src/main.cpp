#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>
#include <SparkFun_AS7343.h> 

// --- NETWORK CONFIGURATION ---
const char* WIFI_SSID = "Leaf-Link";
const char* WIFI_PASS = "cuvettemaster";
const char* MQTT_SERVER = "10.42.0.1";
const int MQTT_PORT = 1883;

// --- PIN DEFINITIONS (Forced Custom PCB Routing) ---
#define SDA_PIN 21    // Hardware TX0
#define SCL_PIN 22
#define THERMISTOR_PIN 34  
#define MOSFET_PIN 13

// --- OBJECT INSTANTIATION ---
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SHT31 sht31 = Adafruit_SHT31();
SfeAS7343ArdI2C mySensor; 

// --- CONSTANTS (Thermistor) ---
const float VCC = 3.30;
const float SERIES_R = 10000.0;
const float R0 = 10000.0;
const float NOMINAL_TEMP = 25.0 + 273.15; 
const float BETA = 3950.0;

// --- VARIABLES FOR MULTITASKING & AVERAGING ---
unsigned long lastMsgTime = 0;
const long interval = 5000; 
int sampleCount = 0;

float sumShtTemp = 0;
float sumShtHum = 0;
float sumThermTemp = 0;
long sumSpecBlu = 0;
long sumSpecGrn = 0;
long sumSpecRed = 0;
unsigned long sumSpecTotal = 0; 

// --- VARIABLES FOR MOSFET TIMING (Dynamic) ---
unsigned long previousMosfetTime = 0;
unsigned long mosfetOnDuration = 13 * 60000; 
unsigned long mosfetOffDuration = 2 * 60000; 
bool mosfetIsOn = false;

// --- HELPER: THERMISTOR ---
float getThermistorTemp() {
  int adcVal = analogRead(THERMISTOR_PIN);
  if (adcVal <= 0) return 0.0;
  float voltage = adcVal * (VCC / 4095.0);
  float Rtherm = (SERIES_R / ((VCC / voltage - 1.0)));
  float steinhart = log(Rtherm / R0) / BETA + 1.0 / NOMINAL_TEMP;
  float tempC = (1.0 / steinhart) - 273.15;
  //return tempC;
  return (((tempC - 32.21) * 30.4) / 14.29) + 1.9;
}

// --- HELPER: RAW I2C OVERRIDE ---
void writeSpectralReg(byte reg, byte val) {
  Wire.beginTransmission(0x39);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// --- HELPER: I2C BUS RECOVERY FOR TX0 PIN ---
void clearI2CBus() {
  // 1. Explicitly kill the Serial port so it releases Pin 1
  Serial.end(); 
  
  // 2. Take manual GPIO control of the pins
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(200); // Wait for the boot ROM garbage to completely finish

  // 3. Generate 16 manual clock pulses to free any stuck sensors
  pinMode(SCL_PIN, OUTPUT);
  for (int i = 0; i < 16; i++) {
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(20);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(20);
  }

  // 4. Force a clean I2C STOP condition
  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(20);
  digitalWrite(SCL_PIN, HIGH); 
  delayMicroseconds(20);
  digitalWrite(SDA_PIN, HIGH); 
  delay(100);
}

// --- MQTT CALLBACK ---
void callback(char* topic, byte* message, unsigned int length) { 
  String msg;
  for (int i = 0; i < length; i++) msg += (char)message[i];

  if (String(topic) == "sensors/leaf_1/control") {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) {
      if (doc["open"].is<unsigned long>()) {
        mosfetOnDuration = doc["open"].as<unsigned long>() * 60000;
      }
      if (doc["closed"].is<unsigned long>()) {
        mosfetOffDuration = doc["closed"].as<unsigned long>() * 60000;
      }
    }
  }
}

// --- NETWORK SETUP ---
void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA); 
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32LeafClient")) {
      client.subscribe("sensors/leaf_1/control");
    } else {
      delay(5000);
    }
  }
}

// --- MAIN SETUP ---
void setup() {
  // 1. Recover the deadlocked bus BEFORE starting the Wire library!
  // Notice there is NO Serial.begin() anywhere!
  clearI2CBus();

  // 2. Initialize the bus safely on the custom pins
  Wire.begin(SDA_PIN, SCL_PIN);

  // 3. Force Soft Resets to wipe out any lingering state logic
  Wire.beginTransmission(0x44); 
  Wire.write(0x30);
  Wire.write(0xA2);
  Wire.endTransmission();
  
  Wire.beginTransmission(0x39); 
  Wire.write(0x80);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100); 

  // --- Normal Initialization ---
  pinMode(THERMISTOR_PIN, INPUT);
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);
  mosfetIsOn = false;
  previousMosfetTime = millis();

  sht31.begin(0x44); 

  if (mySensor.begin(0x39, Wire)) {
    mySensor.powerOn();
    writeSpectralReg(0xC6, 0x02); 
    writeSpectralReg(0x81, 0x1D); 
    writeSpectralReg(0xD4, 0xE7); 
    writeSpectralReg(0xD5, 0x03); 
    mySensor.setAutoSmux(AUTOSMUX_18_CHANNELS); 
    mySensor.enableSpectralMeasurement();
  }

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
      digitalWrite(MOSFET_PIN, LOW); 
      mosfetIsOn = false;
      previousMosfetTime = currentMillis;
    }
  } else {
    if (currentMillis - previousMosfetTime >= mosfetOffDuration) {
      digitalWrite(MOSFET_PIN, HIGH); 
      mosfetIsOn = true;
      previousMosfetTime = currentMillis;
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
    
    if (mySensor.readSpectraDataFromSensor()) {
      uint16_t b = mySensor.getBlue();
      uint16_t g = mySensor.getGreen();
      uint16_t r = mySensor.getRed();
      sumSpecBlu += b;
      sumSpecGrn += g;  
      sumSpecRed += r;
      sumSpecTotal += (b + g + r);
    }
    sampleCount++;
  }

  // --- SLOW LOOP (Publish every 5 seconds) ---
  if (currentMillis - lastMsgTime > interval) {
    lastMsgTime = currentMillis;

    if (sampleCount > 0) {
      float avgRawSpectralSum = (float)sumSpecTotal / sampleCount;
      float calibratedPAR = (0.2114 * avgRawSpectralSum) - 0.3242;
      if (calibratedPAR < 0) calibratedPAR = 0.0;

      JsonDocument doc;
      doc["sensor"] = "leaf_node_1";
      doc["cuvette_id"] = 1; 
      
      doc["temp_air"] = sumShtTemp / sampleCount;
      doc["humidity"] = sumShtHum / sampleCount;
      doc["temp_leaf"] = sumThermTemp / sampleCount;
      doc["par_value"] = calibratedPAR; 
      
      doc["mosfet_state"] = mosfetIsOn ? 1 : 0; 
      doc["mosfet_open_min"] = mosfetOnDuration / 60000;
      doc["mosfet_closed_min"] = mosfetOffDuration / 60000;

      char buffer[512];
      serializeJson(doc, buffer);
      
      client.publish("sensors/leaf_1", buffer);
      
      sumShtTemp = 0; sumShtHum = 0; sumThermTemp = 0;
      sumSpecBlu = 0; sumSpecGrn = 0; sumSpecRed = 0; sumSpecTotal = 0;
      sampleCount = 0;
    }
  }
}