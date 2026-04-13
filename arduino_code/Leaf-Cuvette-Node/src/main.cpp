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

// --- PIN DEFINITIONS ---
#define SDA_PIN 1
#define SCL_PIN 2
#define THERMISTOR_PIN 4      
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
const float T0 = 25.0 + 273.15;
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

// --- VARIABLES FOR MOSFET TIMING ---
unsigned long previousMosfetTime = 0;
const long mosfetOnDuration = 780000;  
const long mosfetOffDuration = 120000; 
bool mosfetIsOn = false;

// --- HELPER: THERMISTOR ---
float getThermistorTemp() {
  int adcVal = analogRead(THERMISTOR_PIN);
  if (adcVal <= 0) return 0.0;
  float voltage = adcVal * (VCC / 4095.0);
  float Rtherm = (SERIES_R * (VCC / voltage - 1.0));
  float steinhart = log(Rtherm / R0) / BETA + 1.0 / T0;
  float tempC = (1.0 / steinhart) - 273.15;
  return (((tempC - 32.21) * 30.4) / 14.29) + 1.9;
}

// --- HELPER: RAW I2C OVERRIDE ---
void writeSpectralReg(byte reg, byte val) {
  Wire.beginTransmission(0x39); // AS7343 Address
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// --- MQTT CALLBACK ---
void callback(char* topic, byte* message, unsigned int length) { }

// --- NETWORK SETUP ---
void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to WiFi: "); 
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA); 
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
      Serial.print("failed, try again in 5 seconds");
      delay(5000);
    }
  }
}

// --- MAIN SETUP ---
void setup() {
  Serial.begin(115200);
  
  unsigned long waitStart = millis();
  while (!Serial && millis() - waitStart < 5000) {
    delay(10);
  }
  
  Serial.println("\n\n--- ESP32 AWAKE ---");
  
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(THERMISTOR_PIN, INPUT);
  
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);
  mosfetIsOn = true;
  previousMosfetTime = millis();

  sht31.begin(0x44); 

  if (!mySensor.begin(0x39, Wire)) {
    Serial.println("AS7343 failed!");
  } else {
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
      // Accumulate the raw sum of channels for the PAR calculation
      sumSpecTotal += (b + g + r);
    }
    sampleCount++;
  }

  // --- SLOW LOOP (Publish every 5 seconds) ---
  if (currentMillis - lastMsgTime > interval) {
    lastMsgTime = currentMillis;

    if (sampleCount > 0) {
      // Calculate averages
      float avgRawSpectralSum = (float)sumSpecTotal / sampleCount;
      
      // Apply the calibration equation: PAR = 0.2114 * x - 0.3242
      float calibratedPAR = (0.2114 * avgRawSpectralSum) - 0.3242;
      if (calibratedPAR < 0) calibratedPAR = 0.0; // Prevent negative readings

      JsonDocument doc;
      doc["sensor"] = "leaf_node_1";
      doc["temp_air"] = sumShtTemp / sampleCount;
      doc["humidity"] = sumShtHum / sampleCount;
      doc["temp_leaf"] = sumThermTemp / sampleCount;
      
      doc["spectral_blu"] = (float)sumSpecBlu / sampleCount;
      doc["spectral_grn"] = (float)sumSpecGrn / sampleCount;
      doc["spectral_red"] = (float)sumSpecRed / sampleCount;
      doc["par_value"] = calibratedPAR; 
      doc["mosfet_state"] = mosfetIsOn ? 1 : 0; 

      char buffer[512];
      serializeJson(doc, buffer);
      
      client.publish("sensors/leaf_1", buffer);
      Serial.println("Published: " + String(buffer));
      
      // Reset accumulators
      sumShtTemp = 0; sumShtHum = 0; sumThermTemp = 0;
      sumSpecBlu = 0; sumSpecGrn = 0; sumSpecRed = 0; sumSpecTotal = 0;
      sampleCount = 0;
    }
  }
}