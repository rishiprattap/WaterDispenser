#pragma once
#include <Arduino.h>

// Change ONLY with pump power disconnected; verify relay contacts before use.
constexpr bool RELAY_ACTIVE_LOW = true; // false for an active-HIGH module
constexpr uint8_t RELAY_PIN = D7;       // GPIO13
constexpr uint8_t BUTTON_PIN = D6;      // GPIO12, button to GND
constexpr uint8_t BUZZER_PIN = D5;      // GPIO14
constexpr uint8_t OLED_SDA = D2;        // GPIO4
constexpr uint8_t OLED_SCL = D1;        // GPIO5
constexpr uint8_t OLED_ADDRESS = 0x3C;  // some modules use 0x3D
constexpr uint8_t OLED_ROTATION = 2;    // 180 degrees; 0 restores original orientation
constexpr uint32_t OLED_I2C_HZ = 100000; // conservative clock, including library transfers
constexpr bool BUZZER_ENABLED = true;
constexpr bool PASSIVE_BUZZER = true;  // false for active-HIGH active buzzer
constexpr char AP_SSID[] = "AquaControl";
constexpr char AP_PASSWORD[] = "WaterFill8266"; // CHANGE before normal use, 8-63 characters
constexpr uint32_t MIN_RUN_MS = 500;
constexpr uint32_t MAX_RUN_MS = 30000;
constexpr uint32_t DEFAULT_RUN_MS = 3000;
constexpr uint8_t RELAY_OFF_LEVEL = RELAY_ACTIVE_LOW ? HIGH : LOW;
constexpr uint8_t RELAY_ON_LEVEL = RELAY_ACTIVE_LOW ? LOW : HIGH;
