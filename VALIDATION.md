# Validation and bench acceptance

## Automated software checks

### OLED rotation, diagnostics and recovery v2

- Arduino CLI compilation passed for `esp8266:esp8266:nodemcuv2`, core 3.1.2,
  SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4. RAM: 30,804 / 80,192 bytes;
  instruction RAM including cache: 62,031 / 65,536; flash code: 302,400 / 1,048,576.
  This was a compile only; no firmware was uploaded.
- Dashboard behavior tests passed again for this change.
- Recovery v2 build and source checks passed. Transient-fault recovery, retry
  spacing and deferral during dispensing still require the controlled bench
  checks in OLED_DIAGNOSTICS.md; no hardware recovery result is claimed.
- A source comparison against main commit `1e820e834f41d7d365d0fa075c4293ecb97bfb83`
  confirmed unchanged relay/start/stop/cutoff, button, settings/API, Wi-Fi setup,
  initial relay OFF ordering and existing loop operations. Only the diagnostic
  service call was added to the loop. This is a source check, not hardware timing evidence.
- Physical rotation, persistent display operation, I2C fault behavior and relay
  response times remain unverified. Follow [OLED_DIAGNOSTICS.md](OLED_DIAGNOSTICS.md)
  and the bench matrix below. At 100 kHz, display transfers take longer; measure
  button/web latency under load and faults before normal use.

### Original main revision baseline

- Arduino CLI compilation **passed** for `esp8266:esp8266:nodemcuv2` using ESP8266 core **3.1.2**, Adafruit SSD1306 **2.5.17**, Adafruit GFX **1.12.6**, and Adafruit BusIO **1.17.4**.
- Compiler report: static RAM **30,516 / 80,192 bytes (38%)**; instruction RAM including reserved cache **62,031 / 65,536 bytes (94%)**; flash code **301,376 / 1,048,576 bytes (28%)**. Runtime heap and behavior under real traffic were not measured.
- Dashboard JavaScript was executed in a Node.js test harness with simulated DOM and HTTP responses. Checks passed for initial settings load, unsaved-edit start lock, start request/token, running-state control lock, completion, save body, disconnected/unknown state, accessible Stop while disconnected, no automatic command retries, and text-only rendering of custom strings.
- These are behavior checks, not a real-browser rendering or hardware test.

## Hardware status

**Recovery v2 has not been uploaded or tested by this agent on your physical NodeMCU. Relay hardware remains unverified.** The user's logs from the earlier diagnostic firmware confirm an address NACK followed by a reset and a failed startup probe. No claim is made that recovery v2 fixes the physical fault, the pump switches, or a given time delivers a particular volume.

Perform these checks before normal use, initially with pump power disconnected:

| Check | Expected result |
|---|---|
| Power up / press reset | Relay contacts remain released; dashboard reports ready/OFF |
| Hold D6 button during boot | No run until released and pressed again |
| Press and hold after boot | One run only; holding never repeats |
| Save 0.5 s; start | Relay commands ON then OFF near 0.5 s |
| Save 30 s; start | Relay commands OFF near 30 s |
| Press button during run | Relay commands OFF and state becomes stopped |
| Web Stop during run | Relay commands OFF and state becomes stopped |
| Repeat web Stop | Relay stays OFF |
| Start while already running | Rejected; original deadline is not extended |
| Disconnect phone during run | Timed OFF still occurs; reconnect shows final state |
| Save during run | Rejected; active run and settings are unchanged |
| Submit 0, 499, 30001, negative or nonnumeric duration through API | 400; configuration unchanged |
| Submit overlong text, emoji or newlines | 400; configuration unchanged |
| Submit missing/invalid token | 403; no output or setting change |
| Save duration/text; power-cycle | Settings restored, relay OFF, no resumed run |
| Disconnect OLED before boot | Web/button operation continues; dashboard says OLED missing |
| Test independent relay/NodeMCU supply sequencing | Relay remains released with proper interface and external bias |

After confirming the relay contacts and all stop paths, connect the low-voltage pump, use a collecting container, and supervise initial runs. Check OFF by observing both the electrical contacts and stopped water flow. A welded relay, siphon, or mechanical valve problem cannot be detected by this firmware.

The web API is synchronous; button and browser response times under network load and OLED faults require bench testing. The Ticker OFF callback is an additional software mechanism, not an independent hardware timeout. Choose a suitable hardware cutoff if a missed stop could cause damage.
