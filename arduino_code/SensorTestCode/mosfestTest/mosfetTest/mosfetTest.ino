#include <Arduino.h>

// --- PIN DEFINITIONS ---
#define MOSFET1_PIN 13  // Make sure this matches your physical wiring

unsigned long myTime;

void setup() {
  Serial.begin(115200);
  
  // Set the pin to act as an Output
  pinMode(MOSFET1_PIN, OUTPUT);

  
  // Start with it safely OFF
  digitalWrite(MOSFET1_PIN, LOW);
  
  Serial.println("\n--- MOSFET Hardware Test Started ---");
}

void loop() {
  myTime = millis() / 1000;
  Serial.print("Time since start: ");
  Serial.print(myTime);
  Serial.println("s");
  
  // 1. Turn ON both (Send 3.3V to the Gate)
  Serial.println("System Open");
  digitalWrite(MOSFET1_PIN, HIGH);
  delay(780000); // Wait 13 min
  
  // 2. Turn OFF (Send 0V to the Gate)
  Serial.println("System Closed");
  digitalWrite(MOSFET1_PIN, LOW);
  delay(120000); // Wait 2 min
}
