# Validation and bench acceptance

## Automated software checks

- Arduino CLI compilation **passed** for `esp8266:esp8266:nodemcuv2` using ESP8266 core **3.1.2**, Adafruit SSD1306 **2.5.17**, Adafruit GFX **1.12.6**, and Adafruit BusIO **1.17.4**.
- Compiler report: static RAM **30,516 / 80,192 bytes (38%)**; instruction RAM including reserved cache **62,031 / 65,536 bytes (94%)**; flash code **301,376 / 1,048,576 bytes (28%)**. Runtime heap and behavior under real traffic were not measured.
- Dashboard JavaScript was executed in a Node.js test harness with simulated DOM and HTTP responses. Checks passed for initial settings load, unsaved-edit start lock, start request/token, running-state control lock, completion, save body, disconnected/unknown state, accessible Stop while disconnected, no automatic command retries, and text-only rendering of custom strings.
- These are behavior checks, not a real-browser rendering or hardware test.

## Hardware status

**Not uploaded to or tested on your physical NodeMCU. Relay hardware remains unverified.** No claim is made that the pump switches, the display lights, or a given time delivers a particular volume.

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
