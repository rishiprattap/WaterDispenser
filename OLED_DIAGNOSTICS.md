# OLED blank after about two seconds

## What the source establishes

The inspected main firmware has no two-second timeout, sleep call or runtime
display-off command. It clears the RAM framebuffer, draws text and a border,
then sends the completed frame every 200 ms. Even empty saved messages leave
the IP line and progress border. Therefore the reported symptom is not explained
by an intentional blank frame or screen timeout in this revision. Confirm that
the uploaded sketch is this version by looking for the new `[boot]` banner.

The user's serial capture now confirms an address NACK (`I2C error=2`) at
19,679 ms, followed by `oled=0`. After a reported `External System` reset,
the startup probe also returned 2. This establishes loss of I2C communication,
but does not distinguish wiring, supply, module failure or interference. Whether
the external reset was intentional remains unknown. No voltage measurements
or agent-performed hardware tests are available.

The first diagnostic revision suspended updates until restart. Recovery v2
instead retries at most once every five seconds while stopped, including when
the OLED was missing at boot. It probes before initialization and again afterward,
restores rotation/text settings, and redraws the application screen on success.
It never retries initialization during dispensing or restarts the ESP8266.
There are no retry delay loops, address changes or flash writes. A persistent
hardware fault will still need repair; firmware cannot restore a missing supply.

This patch rotates all rendered content 180 degrees and sets both Adafruit I2C
transfer/restoration clocks to 100 kHz (the transfer default was 400 kHz). This
is a wiring-tolerance mitigation, not proof that clock speed caused the symptom.
It keeps normal redraws; it does not repeatedly issue DISPLAYON to conceal faults.

## Capture evidence

1. Disconnect pump power. Use the existing NodeMCU board settings and dependencies
   in README, Verify, then upload the PR sketch only when ready to bench-test.
   There is no website/filesystem upload. Do not erase saved settings.
2. Open Serial Monitor at **115200 baud**, then press reset once. Save the output
   from boot through at least 30 seconds, including the moment the OLED blanks.
   The ROM boot text may be garbled at this baud; application logs must be readable.
3. Expect `[boot] AquaControl OLED recovery v2` with reset reason/details, `[OLED]` with address, GPIO pins,
   100000 Hz, probe result and initialization status, followed by AP status.
   `[live]` appears about every five seconds with uptime (`ms`), free heap,
   OLED availability, submitted frame count and commanded run state.
4. Join AquaControl and open http://192.168.4.1/. Check the physical display is
   rotated, the IP line/border stay visible for at least five minutes, and the
   web page remains responsive. Save a distinctive title to confirm it is the
   expected firmware and display. Test ready, dispensing, complete and stopped.

## Interpret the evidence

| Observation | What it tells you / next check |
|---|---|
| Repeated `[boot]` banners and uptime restarting | ESP8266 resets. Use the logged reason/exception details to distinguish watchdog/software faults; a reset reason alone cannot prove or exclude a supply dip. Watch whether Wi-Fi also disappears. |
| `[OLED] I2C error=...` while uptime continues | The configured address stopped acknowledging or the bus failed. Frames stop, `oledOK` becomes false and the control loop continues. Idle recovery attempts appear as `[OLED] retry ...`; `init=OK` resumes drawing. Power down before inspecting SDA D2/GPIO4, SCL D1/GPIO5, ground, address and supply. |
| Startup probe nonzero | No successful address probe; initialization is skipped. Check wiring/address and panel supply. |
| Startup/retry probe zero but init FAILED | The Adafruit initialization call failed (for this library, framebuffer allocation can fail). Check memory/dependency setup; address ACK alone is insufficient. |
| Uptime/frame count continues, OLED remains available, but pixels blank | No observed address-probe failure. This does **not** prove successful frame transfers or a powered/healthy panel. Investigate OLED supply/reset, wiring noise and actual controller/type; inspect I2C with a logic analyzer if available. |
| Heartbeats stop without a new boot banner | Possible stall, lost USB/serial connection or power loss; check dashboard and supply before drawing a conclusion. |

The address probe runs before each frame, not during its payload. Adafruit's
`display()` returns no transfer result and ignores Wire errors internally;
`frames` counts submitted frames, not confirmed visible frames. An error only
during a data transfer can escape these probes. No software log measures the
OLED supply or proves a brownout. A scope is more useful than a meter for brief
dips at Wi-Fi startup or relay switching. Compare with a stable USB supply and
the relay/pump load disconnected, then investigate load-related noise separately.
Keep I2C pull-ups at 3.3 V and follow the module's actual supply specification.

## Regression checks before using the pump

Run the full bench matrix in VALIDATION.md. Specifically check a 0.5-second and
30-second run, button Stop, web Stop, held-button boot/release behavior, loss of
phone Wi-Fi during a run, rejected saves/second starts while running, and settings
after reboot. Check relay contacts, not just the dashboard or LEDs.

With power off, disconnect the OLED; boot and verify web/button timing still
works. Reconnect only with power off. Runtime bus-fault injection should use a
controlled test fixture with pump power disconnected; verify the error log,
paused frame writes, continued dashboard/button operation and timed relay OFF.
For a controlled transient fault, verify no initialization retries during a run,
then `[OLED] retry ... init=OK`, restored rotation/text and resumed frame counts
after stopping and restoring communication. For a persistent fault, verify retries
are spaced at least five seconds apart while idle and control remains responsive.
Do not short SDA/SCL to a supply to simulate a fault.

At 100 kHz a full 512-byte framebuffer takes roughly 50 ms including overhead,
longer than at 400 kHz. The existing Ticker cutoff and main-loop safety checks are
unchanged, but button/web latency under display faults and load needs bench
measurement. This remains cooperative software, not a hardware safety interlock.

## Library references

- [Adafruit SSD1306 2.5.17 implementation](https://github.com/adafruit/Adafruit_SSD1306/blob/2.5.17/Adafruit_SSD1306.cpp): transfer clocks, initialization and display writes.
- [ESP8266 core 3.1.2 APIs](https://arduino-esp8266.readthedocs.io/en/3.1.2/libraries.html): Wire and reset diagnostics.
