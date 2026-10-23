/* AquaControl / ESP8266 NodeMCU 1.0. See README.md before connecting a pump.
   Timer only: no flow sensor, no volume measurement, no relay feedback.
   All application timing is cooperative; no delay() or waiting loops.
   Ticker adds a software OFF deadline, not a certified hardware interlock. */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Ticker.h>
#include <stddef.h>
#include "config.h"
#include "dashboard.h"

struct Settings {
  uint32_t magic;
  uint32_t durationMs;
  char text[4][22]; // 21 printable ASCII characters + terminator
  uint32_t checksum;
};
constexpr uint32_t SETTINGS_MAGIC = 0x41515531; // format version 1
Settings settings{};
ESP8266WebServer server(80);
Adafruit_SSD1306 display(128, 32, &Wire, -1);
Ticker cutoff;
volatile bool deadlineFired = false;
bool running = false, oledOK = false, apOK = false;
bool rawButton = HIGH, stableButton = HIGH, buttonArmed = false;
bool buzzing = false, savedOnce = false;
uint32_t buttonChanged = 0, startedAt = 0, runMs = 0, lastElapsed = 0;
uint32_t lastOLED = 0, buzzStarted = 0, lastSave = 0, stoppedAt = 0;
const char* state = "ready";
String storageNote, controlToken;

uint32_t checksum(const Settings& s) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&s);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(Settings, checksum); ++i)
    hash = (hash ^ bytes[i]) * 16777619UL;
  return hash;
}

bool validText(const String& text) {
  if (text.length() > 21) return false;
  for (size_t i = 0; i < text.length(); ++i)
    if (text[i] < 32 || text[i] > 126) return false;
  return true;
}

bool validSettings(const Settings& s) {
  if (s.magic != SETTINGS_MAGIC || s.checksum != checksum(s) ||
      s.durationMs < MIN_RUN_MS || s.durationMs > MAX_RUN_MS) return false;
  for (uint8_t i = 0; i < 4; ++i) {
    if (memchr(s.text[i], '\0', 22) == nullptr || !validText(String(s.text[i]))) return false;
  }
  return true;
}

void loadSettings() {
  EEPROM.begin(sizeof(Settings));
  Settings stored{};
  EEPROM.get(0, stored);
  if (validSettings(stored)) {
    settings = stored;
    storageNote = "Loaded from flash";
  } else {
    settings.magic = SETTINGS_MAGIC;
    settings.durationMs = DEFAULT_RUN_MS;
    strcpy(settings.text[0], "AQUA CONTROL");
    strcpy(settings.text[1], "Ready to dispense");
    strcpy(settings.text[2], "Dispensing...");
    strcpy(settings.text[3], "Timer complete");
    settings.checksum = checksum(settings);
    storageNote = "Defaults loaded; save settings to flash";
  }
}

void beep() {
  if (!BUZZER_ENABLED) return;
  buzzStarted = millis();
  buzzing = true;
  if (PASSIVE_BUZZER) tone(BUZZER_PIN, 2200);
  else digitalWrite(BUZZER_PIN, HIGH);
}

void finishRun(bool completed) {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL); // OFF before any other work
  cutoff.detach();
  if (running) {
    const uint32_t elapsed = millis() - startedAt;
    lastElapsed = completed ? runMs : (elapsed < runMs ? elapsed : runMs);
    state = completed ? "complete" : "stopped";
    stoppedAt = millis();
    running = false;
    beep();
  }
  deadlineFired = false;
}

void serviceSafety() {
  if (running && (deadlineFired || uint32_t(millis() - startedAt) >= runMs)) finishRun(true);
}

bool startRun() {
  serviceSafety();
  if (running || !apOK || uint32_t(millis() - stoppedAt) < 500) return false;
  runMs = settings.durationMs;
  if (runMs < MIN_RUN_MS || runMs > MAX_RUN_MS) return false;
  deadlineFired = false;
  lastElapsed = 0;
  startedAt = millis();
  state = "dispensing";
  running = true;
  // Callback only switches the GPIO and sets a flag: no network/flash/OLED IO.
  cutoff.once_ms(runMs, []() {
    digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
    deadlineFired = true;
  });
  digitalWrite(RELAY_PIN, RELAY_ON_LEVEL);
  beep();
  return true;
}

void serviceButton() {
  const uint32_t now = millis();
  const bool reading = digitalRead(BUTTON_PIN);
  if (reading != rawButton) { rawButton = reading; buttonChanged = now; }
  if (uint32_t(now - buttonChanged) < 35) return;
  // Require a debounced release after boot and after each accepted press.
  if (reading == HIGH) buttonArmed = true;
  if (reading != stableButton) {
    stableButton = reading;
    if (reading == LOW && buttonArmed) {
      buttonArmed = false;
      if (running) finishRun(false);
      else startRun();
    }
  }
}

uint32_t elapsedMs() {
  if (!running) return lastElapsed;
  uint32_t elapsed = millis() - startedAt;
  return elapsed < runMs ? elapsed : runMs;
}

String oledMessage() {
  if (!apOK) return "WiFi AP failed";
  if (running) return String(settings.text[2]);
  if (strcmp(state, "complete") == 0) return String(settings.text[3]);
  if (strcmp(state, "stopped") == 0) return "Stopped manually";
  return String(settings.text[1]);
}

String oledTiming() {
  if (!running) return "IP 192.168.4.1";
  return String(float(runMs - elapsedMs()) / 1000, 1) + "s left / TIMER";
}

void updateOLED() {
  if (!oledOK || uint32_t(millis() - lastOLED) < 200) return;
  lastOLED = millis();
  display.clearDisplay();
  display.setCursor(0, 0); display.print(settings.text[0]);
  display.setCursor(0, 8); display.print(oledMessage());
  display.setCursor(0, 16); display.print(oledTiming());
  display.drawRect(0, 26, 128, 6, SSD1306_WHITE);
  if (runMs) display.fillRect(1, 27, (126UL * elapsedMs()) / runMs, 4, SSD1306_WHITE);
  display.display();
}

String quoted(const String& input) {
  String result = "\"";
  result.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] == '"' || input[i] == '\\') result += '\\';
    result += input[i];
  }
  result += '"';
  return result;
}

void reply(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void errorReply(int code, const char* message) {
  reply(code, "{\"error\":" + quoted(message) + "}");
}

bool authorized() {
  if (server.header("X-Control-Token") != controlToken) {
    errorReply(403, "Refresh the dashboard and try again.");
    return false;
  }
  return true;
}

void sendStatus() {
  serviceSafety();
  String json;
  json.reserve(800);
  json = "{\"state\":" + quoted(state);
  json += ",\"running\":" + String(running ? "true" : "false");
  json += ",\"elapsedMs\":" + String(elapsedMs());
  json += ",\"runMs\":" + String(runMs);
  json += ",\"durationMs\":" + String(settings.durationMs);
  json += ",\"activeLow\":" + String(RELAY_ACTIVE_LOW ? "true" : "false");
  json += ",\"oledOK\":" + String(oledOK ? "true" : "false");
  json += ",\"storage\":" + quoted(storageNote);
  json += ",\"token\":" + quoted(controlToken);
  json += ",\"texts\":[";
  for (uint8_t i = 0; i < 4; ++i) { if (i) json += ','; json += quoted(settings.text[i]); }
  json += "],\"oledLines\":[" + quoted(settings.text[0]) + ',' + quoted(oledMessage()) + ',' + quoted(oledTiming()) + "]}";
  reply(200, json);
}

bool parseDuration(const String& input, uint32_t& result) {
  if (input.length() == 0 || input.length() > 5) return false;
  result = 0;
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] < '0' || input[i] > '9') return false;
    result = result * 10 + (input[i] - '0');
  }
  return result >= MIN_RUN_MS && result <= MAX_RUN_MS;
}

void saveSettings() {
  if (!authorized()) return;
  serviceSafety();
  if (running) { errorReply(409, "Stop dispensing before saving settings."); return; }
  if (server.args() != 5) { errorReply(400, "Expected durationMs and text0 through text3."); return; }
  Settings next{};
  next.magic = SETTINGS_MAGIC;
  if (!parseDuration(server.arg("durationMs"), next.durationMs)) {
    errorReply(400, "Duration must be an integer from 500 to 30000 milliseconds."); return;
  }
  for (uint8_t i = 0; i < 4; ++i) {
    String key = "text" + String(i);
    if (!server.hasArg(key) || !validText(server.arg(key))) {
      errorReply(400, "Each text must contain at most 21 printable ASCII characters."); return;
    }
    server.arg(key).toCharArray(next.text[i], 22);
  }
  next.checksum = checksum(next);
  if (memcmp(&next, &settings, sizeof(Settings)) == 0 && storageNote == "Loaded from flash") {
    sendStatus(); return;
  }
  if (savedOnce && uint32_t(millis() - lastSave) < 5000) {
    errorReply(429, "Please wait five seconds between saves."); return;
  }
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  EEPROM.put(0, next);
  savedOnce = true; lastSave = millis();
  if (!EEPROM.commit()) {
    EEPROM.put(0, settings); // retain current runtime configuration on failure
    storageNote = "Flash save failed; previous runtime settings retained";
    errorReply(500, "Flash save failed. Settings were not applied."); return;
  }
  settings = next;
  storageNote = "Loaded from flash";
  sendStatus();
}

void setup() {
  // First application operation: preload the OFF latch before enabling output.
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  rawButton = stableButton = digitalRead(BUTTON_PIN);
  buttonChanged = millis();
  Serial.begin(115200);
  loadSettings();
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClockStretchLimit(1500);
  Wire.beginTransmission(OLED_ADDRESS);
  const bool detected = Wire.endTransmission() == 0;
  oledOK = detected && display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, false, false);
  if (oledOK) {
    display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setTextWrap(false);
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  apOK = WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  apOK = apOK && WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 4);
  controlToken = String(ESP.random(), HEX) + String(ESP.random(), HEX);
  server.collectHeaders("X-Control-Token");
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Options", "DENY");
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
  });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/settings", HTTP_POST, saveSettings);
  server.on("/api/start", HTTP_POST, []() {
    if (!authorized()) return;
    if (!startRun()) { errorReply(409, "Already running, cooling down, or unavailable."); return; }
    sendStatus();
  });
  server.on("/api/stop", HTTP_POST, []() {
    if (!authorized()) return;
    finishRun(false); sendStatus();
  });
  server.onNotFound([]() { errorReply(404, "Not found"); });
  if (apOK) server.begin();
  Serial.printf("\nAquaControl: AP %s; OLED %s; relay active %s\n", apOK ? "ready" : "FAILED", oledOK ? "detected" : "missing", RELAY_ACTIVE_LOW ? "LOW" : "HIGH");
  Serial.printf("Wi-Fi: %s\nOpen http://192.168.4.1\n", AP_SSID);
}

void loop() {
  serviceSafety();
  serviceButton();
  if (apOK) server.handleClient();
  serviceSafety();
  serviceButton();
  if (buzzing && uint32_t(millis() - buzzStarted) >= 90) {
    if (PASSIVE_BUZZER) noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW); buzzing = false;
  }
  updateOLED();
  yield();
}
