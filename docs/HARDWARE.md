# Selected Payload Electronics

Source: `docs/Payload_Electronics_Summer_Assignment (1).md` (Payload
Electronics component research, summer year 2). This file is the software
side's copy of what was selected, so the constants in
`include/payload/Config.hpp` have a traceable origin.

Status: parts are **selected**; the wiring/bus assignment below is
**proposed** by software and still needs to be confirmed against the real
schematic before anyone cuts a board.

## Parts

| Function | Selected part | Interface | Key numbers | Backup |
|---|---|---|---|---|
| Pressure + temperature | **BMP390** (Adafruit QT breakout) | I2C0 | ±3 Pa relative (≈25 cm altitude), up to 200 Hz ODR | MS5611 |
| Vibration / shock | **ADXL375** | SPI | ±200 g, ≈1 kHz bandwidth, ≈145 µA | ICM-42688-P (adds attitude, but ±16 g clips) |
| Humidity + temperature | **SHT40** | I2C1 (separate bus from the BMP390) | ±1.8 % RH, ±0.2 °C | BME280 |
| Microcontroller | **Teensy 4.1** | — | 600 MHz, 1 MB RAM, 8 MB flash, built-in SD slot, no radio | ESP32 (if wireless telemetry or tight budget) |
| Storage | **microSD, 8–32 GB** | Built-in Teensy SDIO slot | ≈45 MB per flight as CSV | — |

**GPS was removed from the payload** after the component research was
written. The u-blox NEO-M8N appears in `docs/Payload_Electronics_Summer_Assignment (1).md`
as the selected receiver; it is no longer part of the build, and the
software no longer logs a position stream. What went with it:

- No independent altitude source. Barometric altitude from the BMP390 is
  now the only altitude measurement on the payload. **This is accepted:**
  flight states are detected from barometric data, every disreef trigger is
  an altitude threshold, and the per stage backup timers are the failsafe
  if that path is lost (FSD-5, REEF-4). No second barometer is planned.
- No position or ground track, so wind speed and direction can no longer be
  derived from horizontal drift under parachute, which the research doc
  listed as a GPS objective.

Notes carried over from the electronics research that affect software:

- The BMP390 measures **both** pressure and temperature, so those two
  streams come off one chip and share one conversion schedule. It replaced
  the MS5611 after the original component research; the MS5611 is now the
  listed backup.
- The ADXL375's ±200 g range is the reason vibration will not clip at
  ignition or parachute deployment. Bounds in software must match the
  datasheet range, not the old ±16 g placeholder.
- The Teensy 4.1 has no onboard radio, which keeps RF noise away from the
  altimeter, and it timestamps every sample off one shared clock so the
  pressure and vibration streams stay aligned.

## Proposed bus assignment

The electronics research lists shared-bus lockup as a risk and recommends
splitting sensors across separate buses. Proposed split:

| Device | Bus | Teensy 4.1 pins |
|---|---|---|
| BMP390 | Wire (I2C0) | 18 SDA / 19 SCL |
| SHT40 | Wire1 (I2C1) | 17 SDA1 / 16 SCL1 |
| ADXL375 | SPI (SPI0) | 11 MOSI / 12 MISO / 13 SCK, CS 10 |
| microSD | Built-in SDIO slot | dedicated, no pin sharing |

The barometer and the humidity sensor are on **separate I2C buses** on
purpose: a hung SHT40 must not be able to stall the BMP390, because the
barometer is the only altitude source on the payload and every flight state
transition and reefing trigger is derived from it. The ADXL375 has SPI to
itself, and the microSD uses the Teensy's dedicated slot rather than
sharing a sensor bus.

## Sampling and storage budget

| Stream | Rate | Per 60 s flight, CSV |
|---|---|---|
| Pressure (BMP390) | 100 Hz | ≈6,000 rows |
| Temperature (BMP390) | 100 Hz | ≈6,000 rows |
| Vibration (ADXL375) | 800 Hz | ≈48,000 rows |
| Humidity (SHT40) | 1 Hz | ≈60 rows |

Measured against the real program (`data/sample_payload_log.csv`, a 30-second
run): ≈31 KB/s, or ≈956 KB per 30 s, down from ≈37 KB/s before the GPS
streams were removed. The electronics research's ≈30–45 MB
per flight therefore corresponds to roughly 15–20 minutes of recording --
pad arming through recovery -- which is the realistic window. Either way an
8–32 GB card holds a full season many times over, so the readability of
plain-text CSV is worth the size for now.

One caveat on that budget: the ADXL375 is a **three-axis** accelerometer,
and both the research's estimate and this software currently count a single
vibration channel. Logging X, Y and Z separately triples the vibration data
rate (≈100 KB/s total). That is still comfortable for the card, but it is an
open decision, not an oversight -- see SEN-12 in `docs/REQUIREMENTS.md`.

## Risks this drives in software

| Risk (from electronics research) | Software response |
|---|---|
| Sensor clipping | Bounds in `Config.hpp` come from datasheet ranges (±200 g, not ±16 g); out-of-range readings are logged with `OUT_OF_RANGE`, not dropped. |
| Power sag at ignition | Sampling loop and logger both survive a sensor dropping out mid-run; the file is flushed on a fixed interval so a brownout loses at most one flush window. |
| Slow SD writes | `DataLogger` buffers in RAM and flushes every `kFlushIntervalMs` instead of every row, which 800 Hz makes mandatory. |
| Shared-bus lockup | Bus split above; one offline sensor never stops the others (ERR-4). |
| Corrupted log file | Periodic flush plus a guaranteed `close()` on every exit path (ERR-5). |
| Barometer is a single point of failure for flight state and reefing | Accepted, with the per stage backup timers as the failsafe (REEF-4). The BMP390 gets an I2C bus to itself so nothing else on the board can stall it. |

## Still open with Payload Electrical

- How recording starts and stops in flight (`CTL-3`): ground command,
  power-on, deployment switch, or timer. Still the biggest open item.
- Final battery and regulator sizing, once real power draw is measured on
  the breadboard. Removing the NEO-M8N takes ≈31 mA of tracking current off
  the budget, which is most of the sensor-side draw.
- Confirmation of the pin assignment above against the actual schematic.
- Reefing motor, gearbox, and spool selection, and how the target position
  is confirmed — encoder, limit switch, or stall current (REEF-7). This is
  the largest remaining hardware unknown now that the sensor set is settled.
