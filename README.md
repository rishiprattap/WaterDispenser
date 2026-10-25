# AquaControl — ESP8266 water dispenser

A self-contained NodeMCU Arduino project with a mobile-responsive local dashboard, OLED mirror, start/stop controls, a physical start/stop button, buzzer feedback, and flash-persisted settings. No cloud, CDN, external fonts, filesystem upload, or ArduinoJson dependency.

## Read first

**Your relay hardware is currently unverified.** The earlier report of a blinking red LED and no green LED does not identify the module's trigger polarity or establish that its contacts switch. The dashboard reports the commanded output, not measured relay or pump state.

**This is a timer, not a volume meter.** Progress is simulated from elapsed time. Three seconds, or any other duration, does not inherently equal 250 mL. Flow changes with pump, supply, water head and plumbing. Use a measuring container for rough calibration; use a flow sensor and appropriate control design for volume measurement.

Keep pump power disconnected during initial upload and relay testing. Use an appropriately rated low-voltage DC pump/valve and supply. Do not power a pump or relay coil from a GPIO or the NodeMCU 3.3 V regulator. Mains wiring is outside this project's scope.

## Repository layout

```text
WaterDispenser/
  WaterDispenser.ino
  config.h
  dashboard.h
README.md
VALIDATION.md
tests/check-dashboard.cjs
```

Download this repository using **Code → Download ZIP**, extract it, and open `WaterDispenser/WaterDispenser.ino`. All three sketch files must stay together. The outer extracted GitHub folder may have any name.

To run the optional dashboard behavior tests with Node.js, run `node tests/check-dashboard.cjs` from the repository root.

## Files

- `WaterDispenser.ino`: firmware and HTTP API.
- `config.h`: pins, relay polarity, Wi-Fi credentials, buzzer and time limits.
- `dashboard.h`: complete embedded HTML, CSS and JavaScript.
- `README.md`: these instructions.
- `VALIDATION.md`: build results and bench checks.

Keep the first three files together in a folder named **WaterDispenser**. Open `WaterDispenser.ino` in Arduino IDE. The `.h` files will appear as tabs.

## Wiring

Power down before changing wiring.

| Device connection | NodeMCU connection | Notes |
|---|---|---|
| OLED SDA | D2 / GPIO4 | 128×32 SSD1306 I2C |
| OLED SCL | D1 / GPIO5 | Address 0x3C by default |
| OLED GND | GND | Common signal ground |
| OLED VCC | 3V3 if supported by your module | Ensure I2C pull-ups are to 3.3 V, never 5 V |
| Push button | D6 / GPIO12 to GND | Normally open, `INPUT_PULLUP`; no external pull-up needed |
| Buzzer control | D5 / GPIO14 | Small GPIO-compatible passive piezo by default; use a transistor driver for higher current devices |
| Relay IN | D7 / GPIO13 | Must accept 3.3 V logic; verify module specifications |
| Relay power | Rated external supply | Follow the actual module's VCC/JD-VCC and isolation wiring |
| Relay signal GND | NodeMCU GND for a non-isolated input | Do not guess isolated module jumper wiring |

For an ordinary low-voltage DC load: supply positive → relay COM; relay NO → pump positive; pump negative → supply negative. Use **NO**, not NC, so the load is disconnected when the relay is released. Fit appropriate fuse and suppression for the pump/valve; keep motor current out of signal wiring. This contact wiring does not establish that the module input is electrically compatible.

## Relay polarity and boot behavior

`config.h` defaults to `RELAY_ACTIVE_LOW = true`: D7 HIGH commands OFF; D7 LOW commands ON. For a confirmed active-HIGH module set it to `false`, then re-upload with pump power disconnected. Polarity is deliberately a firmware setting, not a web toggle: the firmware must know the OFF level before reading flash.

The first operation in `setup()` preloads the OFF output latch, then makes D7 an output. Every reset starts stopped; a saved duration never resumes a run. A button held at boot must be released before it can start dispensing.

**Software cannot guarantee OFF before `setup()`, during reset, while unpowered, or with a miswired/incompatible relay.** D7 can float before firmware takes control. For a compatible 3.3 V input, use an external inactive-state bias (typically a 10 kΩ pull-up to 3.3 V for active LOW, or pull-down to GND for active HIGH). Check the module circuit first: some 5 V modules require a proper transistor/level interface and can feed unsafe voltage into the ESP8266. A resistor alone does not make such a module compatible. Verify power-up, reset and independent supply sequencing with the pump disconnected. Use a hardware enable/interlock if unintended flow could cause damage.

## Install and upload

1. Install Arduino IDE. In Preferences, add the ESP8266 Boards Manager URL: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`.
2. In Boards Manager install **esp8266 by ESP8266 Community**. This project targets core **3.1.2**.
3. In Library Manager install **Adafruit SSD1306**, **Adafruit GFX Library**, and **Adafruit BusIO** (accept dependency installation). ESP8266WiFi, ESP8266WebServer, EEPROM, Wire and Ticker are included with the board core.
4. Select **NodeMCU 1.0 (ESP-12E Module)** and the board's USB port. For a typical 4 MB NodeMCU use the 4 MB flash setting. Keep the flash layout consistent to retain settings. A slower upload speed such as 115200 can help unreliable USB connections.
5. Edit `AP_PASSWORD` in `config.h` before normal use (8–63 characters). Check relay polarity, OLED address and buzzer type. Defaults are listed below.
6. Click Verify, then Upload. No separate webpage or filesystem upload is needed.
7. Open Serial Monitor at **115200 baud** for AP, OLED and polarity diagnostics. The short ROM boot message may appear garbled at this baud; subsequent application messages should be readable.

## Connect your phone

1. Power the NodeMCU.
2. Join Wi-Fi **AquaControl** using **WaterFill8266**, unless you changed these values.
3. Choose **stay connected / use without internet** if your phone warns that this network has no internet. Temporarily disable mobile-data switching or VPN if it redirects local traffic.
4. Open **http://192.168.4.1/** explicitly in your browser. Use HTTP, not HTTPS. There is no captive-portal redirect.

The ESP8266 creates its own access point; it does not join your home router. Up to four clients may connect. Anyone with the Wi-Fi password can control the dispenser; the HTTP API has no separate user login. Do not bridge or forward it to the internet. A per-boot control token and no CORS permission help prevent ordinary cross-origin browser control; they are not user authentication.

## Using the dashboard

- Set run duration from **0.5 to 30 seconds**, then choose **Save to device**. The UI uses 0.1-second steps; the API accepts whole milliseconds.
- Customize title, ready, dispensing and completion messages. Each allows 0–21 printable ASCII characters. Emoji, non-ASCII letters and line breaks are rejected to match the built-in OLED font. The remaining-time/IP line, progress bar and manual-stop message are fixed.
- The OLED mirror shows the text being sent, with a simulated timer bar. It is not a readback of the physical screen. The OLED is rotated 180 degrees (`OLED_ROTATION = 2`). Its address is checked before every frame; a failed probe disables updates and marks it unavailable until restart. Reconnect only with power off.
- Click **Start dispensing** or press the physical button once. Press **Stop now** or press the button again to stop. Release between button presses. Holding the button does not repeatedly start cycles.
- Saving is disabled during a run and rejected by the device. Unsaved web edits also disable web Start; the physical button always uses the saved/current device configuration.
- The device refuses a second Start while running. It also has a 500 ms cooldown after stopping. A new run requires a new command; there is no automatic restart.
- Closing the page or losing Wi-Fi **does not stop a run**. The device's timer continues and commands OFF at its deadline. If the dashboard reports disconnection, use the physical button; for uncertain hardware operation disconnect pump power.
- Browser commands are not automatically retried. If a response is lost, a command may already have reached the device. Check status before starting again.

## Persistence and timing details

Settings are stored in flash-backed EEPROM only on explicit Save. A format identifier, bounds checks, null-termination checks, printable-text checks and checksum validate data on boot. Invalid or missing data loads a safe 3-second default and displays a defaults notice. First Save stores it. Unchanged saves are skipped after a successful load/save, and changed saves are limited to one per five seconds to reduce flash wear. Save errors are reported without applying the new runtime configuration.

A power interruption during a flash erase/write can lose the saved configuration; the checksum allows most corruption to be detected and defaults loaded. It is not a redundant transactional store. Flash writes are allowed only with the relay commanded OFF. Erasing all flash or changing the flash layout can erase settings.

Application timing uses rollover-safe `millis()` subtraction and no `delay()` or waiting loops. Button debounce is 35 ms, buzzer feedback about 90 ms, OLED updates every 200 ms, and web status polls about every 700 ms. The browser interpolates progress between replies and marks stale status unknown.

Each run snapshots the duration (bounded to 30,000 ms). The main loop checks the deadline before and after HTTP handling. A separate ESP8266 software Ticker callback also commands OFF at the selected deadline and sets a flag for normal-loop cleanup. Network and I2C library operations can still take time; this is cooperative firmware, **not hard real-time control**. The software timer and watchdog are not substitutes for an independent hardware cutoff, and cannot detect welded contacts or continuing flow.

## Troubleshooting the current relay issue

1. Leave pump power disconnected. Check the exact relay module model, supply voltage, input circuit, trigger selection and any JD-VCC jumper requirements.
2. Confirm a steady rated supply and correct signal grounding. A blinking power LED may be a supply/reset problem; LED meanings vary between boards.
3. With a meter, check D7 relative to NodeMCU GND: default idle should be near 3.3 V, dispensing near 0 V. The dashboard is not a voltage measurement.
4. Test a **0.5-second** run and manual Stop. Check relay COM–NO continuity while the contacts are disconnected from load power. A click/LED alone is insufficient proof.
5. If D7 changes but contacts do not, investigate relay power, input compatibility, polarity or a defective module. Do not keep changing timing to solve an electrical problem.
6. Verify relay OFF at boot, during reset, after timeout, after manual Stop and after power loss/recovery before connecting the pump. If operation is inverted, disconnect pump power and correct polarity/wiring.

OLED blank: follow [the OLED diagnostic procedure](OLED_DIAGNOSTICS.md). Check 128×32 SSD1306 type, SDA/SCL order, supply and address; change `OLED_ADDRESS` to 0x3D only if appropriate. Missing OLED does not disable timed control. Buzzer wrong/silent: set `PASSIVE_BUZZER` for your device or set `BUZZER_ENABLED = false`; an active buzzer here must use an active-HIGH input.

## HTTP API

`GET /api/status` returns current commanded state, elapsed/run/configured milliseconds, text settings, OLED lines/availability result, persistence notice, polarity and per-boot token. OLED availability latches false after a failed address probe; true is not pixel readback or proof that every frame transferred. State is `ready`, `dispensing`, `complete` or `stopped`.

All mutations require `X-Control-Token` from status and use POST. No CORS access is granted.

| Endpoint | Body (`application/x-www-form-urlencoded`) |
|---|---|
| `/api/start` | Empty; uses saved duration |
| `/api/stop` | Empty; repeated Stop is safe |
| `/api/settings` | Exactly `durationMs`, `text0`, `text1`, `text2`, `text3` |

Validation errors: 400; missing/stale token: 403; run/cooldown conflict: 409; save rate limit: 429; flash failure: 500. Successful calls return status JSON. Duration is digits only, 500–30000. Unknown routes/methods return 404.

## Primary references

- [ESP8266 access-point API](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html)
- [ESP8266 EEPROM, Wire and Ticker documentation](https://arduino-esp8266.readthedocs.io/en/latest/libraries.html)
- [Adafruit SSD1306 library](https://github.com/adafruit/Adafruit_SSD1306)
