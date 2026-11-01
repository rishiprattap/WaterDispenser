/*
  AquaControl relay-only diagnostic (ESP8266 NodeMCU).
  IMPORTANT: DISCONNECT PUMP POWER AND REMOVE ALL LOAD WIRES FROM COM/NO/NC.
  The relay board must have a compatible supply and input interface.
  Does not use the full AquaControl firmware, OLED, Wi-Fi, or EEPROM.
  Inspect the actual module's polarity: changing the setting does not fix
  a pump wired around the relay, NC wiring, or welded contacts.
*/
#include <Arduino.h>

constexpr uint8_t RELAY_PIN = D7; // GPIO13
constexpr bool RELAY_ACTIVE_LOW = true; // set false ONLY if module confirmed active-HIGH
constexpr uint8_t OFF_LEVEL = RELAY_ACTIVE_LOW ? HIGH : LOW;
constexpr uint8_t ON_LEVEL = RELAY_ACTIVE_LOW ? LOW : HIGH;

bool testing = false;
uint32_t testStarted = 0;
constexpr uint32_t TEST_TIME_MS = 500;

void relayOff() {
  digitalWrite(RELAY_PIN, OFF_LEVEL);
  testing = false;
}

void setup() {
  // Preload the inactive level before configuring the pin as OUTPUT.
  digitalWrite(RELAY_PIN, OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT);
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Relay-only diagnostic. PUMP POWER MUST BE DISCONNECTED."));
  Serial.println(F("Measure D7 relative to NodeMCU GND."));
  Serial.print(F("Configured idle D7: "));
  Serial.println(OFF_LEVEL == HIGH ? F("HIGH (~3.3V)") : F("LOW (~0V)"));
  Serial.println(F("Send T for a single 0.5-second relay test; O forces OFF."));
  Serial.println(F("The relay power LED may stay lit while relay is OFF."));
}

void loop() {
  if (testing && (uint32_t)(millis() - testStarted) >= TEST_TIME_MS) {
    relayOff();
    Serial.println(F("OFF: test completed"));
  }

  while (Serial.available()) {
    char command = (char)Serial.read();
    if (command == 'O' || command == 'o') {
      relayOff();
      Serial.println(F("OFF requested"));
    } else if ((command == 'T' || command == 't') && !testing) {
      digitalWrite(RELAY_PIN, ON_LEVEL);
      testing = true;
      testStarted = millis();
      Serial.println(F("TEST ON for 500ms, then OFF"));
    }
  }
}
