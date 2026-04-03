int thermPin = 4;
const float VCC = 3.30;            // supply voltage used for divider
const float SERIES_R = 10000.0;    // fixed resistor value in ohms
const float R0 = 10000.0;          // thermistor nominal resistance at T0
const float T0 = 25.0 + 273.15;    // nominal temperature (K)
const float BETA = 3950.0;         // beta coefficient

void setup() {
  Serial.begin(115200);
  delay(200);
}

void loop() {
  int raw = analogRead(thermPin);
  // protect against division by zero
  if(raw <= 0){
    Serial.println("RAW==0 -> check wiring (NODE may be grounded)");
    delay(500);
    return;
  }
  if(raw >= 4095){
    Serial.println("RAW==4095 -> check wiring (NODE may be VCC)");
    delay(500);
    return;
  }

  float voltage = (raw / 4095.0) * VCC;
  // voltage across series resistor node = Vnode
  float Rtherm = (SERIES_R * (VCC / voltage - 1.0)); // Rtherm = R * (Vcc/Vnode - 1)
  // Steinhart-Hart (Beta) approximation
  float steinhart = log(Rtherm / R0) / BETA + 1.0 / T0;
  steinhart = 1.0 / steinhart;
  float tempC = steinhart - 273.15;

  Serial.print("RAW: "); Serial.print(raw);
  Serial.print("  Vnode: "); Serial.print(voltage, 3);
  Serial.print(" V  Rtherm: "); Serial.print(Rtherm, 0);
  Serial.print(" ohm  Temp: "); Serial.print(tempC, 2);
  Serial.println(" C");

  delay(500);
}
