# Payload Software Requirements

Status: updated after Payload Electronics selected the flight hardware (see
`docs/HARDWARE.md`). This list is the working baseline for the data
collection framework. It grows or shrinks as scope changes, and every task
in the repo should trace back to at least one requirement below.

Legend for status: `BASELINE` = decided and implemented now, `TBD` = open,
pending coordination with Payload Electrical, `ASSUMED` = a placeholder value
picked so development can proceed, expected to be revisited.

## 1. Sensing

| ID | Requirement | Status |
|----|-------------|--------|
| SEN-1 | The system shall read a pressure sensor at a defined sampling rate. | BASELINE |
| SEN-2 | The system shall read a temperature sensor at a defined sampling rate. | BASELINE |
| SEN-3 | The system shall read a vibration sensor at a defined sampling rate, independent of and normally faster than the pressure and temperature rate. | BASELINE |
| SEN-4 | Pressure and temperature shall sample at 100 Hz, inside the BMP390's 200 Hz ODR. | BASELINE |
| SEN-5 | Vibration shall sample at 800 Hz, inside the ADXL375's ≈1 kHz bandwidth. | BASELINE |
| SEN-6 | Each sensor's sampling rate shall be a single named constant, changeable without touching unrelated code. | BASELINE |
| SEN-7 | The physical sensor models and their communication interfaces shall be finalized with Payload Electrical. | BASELINE — BMP390 (I2C0), SHT40 (I2C1), ADXL375 (SPI); see docs/HARDWARE.md |
| SEN-8 | The system shall read relative humidity from the SHT40 at 1 Hz. | BASELINE |
| SEN-9 | *Withdrawn.* Position logging from the NEO-M8N was removed with the GPS receiver; the payload no longer measures position. | REMOVED |
| SEN-10 | Out of range bounds shall come from the selected parts' datasheets, not from estimates (e.g. ±200 g for the ADXL375, not ±16 g). | BASELINE, see `include/payload/Config.hpp` |
| SEN-11 | *Withdrawn with SEN-9.* No receiver, no loss-of-lock handling. | REMOVED |
| SEN-12 | The ADXL375 measures three axes; this software currently logs one vibration channel. Whether X/Y/Z are each logged (tripling the vibration data rate) shall be decided with Payload Electrical. | TBD |

Driver level requirements for each part (BMP390, ADXL375, SHT40) are in
`docs/SENSOR_REQUIREMENTS.md`.

## 2. Timestamping and Data Format

| ID | Requirement | Status |
|----|-------------|--------|
| FMT-1 | Every measurement shall be timestamped. | BASELINE |
| FMT-2 | Timestamps shall be elapsed **microseconds** since recording start, monotonic and not subject to wall clock adjustment. Milliseconds are too coarse: 800 Hz is a 1.25 ms period. | BASELINE |
| FMT-3 | Saved data shall include, at minimum: time, pressure, temperature, vibration, and a sensor/storage status or error indicator. | BASELINE |
| FMT-4 | Data shall be stored in CSV format with a header row naming each column. | BASELINE |
| FMT-5 | Because sensors sample at different rates, the CSV shall carry a `sensor` column identifying which reading a row belongs to, rather than forcing all streams onto one fixed width row. | BASELINE, see docs/DATA_FORMAT.md |
| FMT-6 | All sensors shall be timestamped off one shared clock so streams stay aligned for analysis (the Teensy 4.1 makes this free). | BASELINE |
| FMT-7 | The CSV shall preserve enough significant digits that the BMP390's ±3 Pa relative accuracy survives the write (7 digits gives 0.1 Pa granularity on a ~101.3 kPa reading). | BASELINE |

## 3. Storage

| ID | Requirement | Status |
|----|-------------|--------|
| STO-1 | Data shall be saved to the onboard storage method selected by the system integration team. | BASELINE — microSD (8–32 GB) in the Teensy 4.1's built in slot |
| STO-2 | On a development machine the program shall write to a local filesystem path, so the same code runs on the bench and on the flight target. | BASELINE |
| STO-3 | The program shall create a new, uniquely named data file per recording session so prior sessions are never overwritten. | BASELINE |
| STO-4 | Available onboard memory and write throughput limits shall be confirmed with Payload Electrical so buffering/flush strategy can be sized correctly. | BASELINE — CSV measures ≈37 KB/s at the rates above, against an 8–32 GB card; rows are buffered and flushed every `kFlushIntervalMs` rather than per row |

## 4. Error Handling and Fault Tolerance

| ID | Requirement | Status |
|----|-------------|--------|
| ERR-1 | The system shall detect a sensor read failure (timeout, out of range value, or communication error) and record it rather than silently dropping data. | BASELINE |
| ERR-2 | The system shall detect a storage write failure and report it. | BASELINE |
| ERR-3 | Each row shall carry a per row status/error code, so post flight analysis can distinguish good data from faulted data without losing the row. | BASELINE |
| ERR-4 | If a sensor stops responding, the system shall continue collecting from the remaining sensors rather than halting the whole program. | BASELINE |
| ERR-5 | If a sensor stops responding or the program is asked to stop, the system shall safely close (flush and close) the data file so it is never left corrupt or truncated mid write. | BASELINE |
| ERR-6 | A power dip at ignition shall cost at most one flush window of data, not the whole file. | BASELINE in software; needs bench confirmation against the real regulator |

## 5. Recording Control

| ID | Requirement | Status |
|----|-------------|--------|
| CTL-1 | The system shall support an explicit start of recording event. | BASELINE |
| CTL-2 | The system shall support an explicit stop of recording event that safely closes the data file. | BASELINE |
| CTL-3 | How recording is triggered in flight (ground command, power on, deployment switch, timer) shall be defined with Payload Electrical/systems. | TBD — see also FSD-6, which proposes flight state detection as the answer |
| CTL-4 | Until CTL-3 is resolved, the ground/dev version shall start on program launch and stop on a keyboard interrupt (Ctrl+C) or a fixed run duration, both of which invoke the same safe shutdown path. | ASSUMED |

## 6. Platform

| ID | Requirement | Status |
|----|-------------|--------|
| PLT-1 | The onboard microcontroller/SBC and its programming language shall be selected jointly with Payload Electrical. | BASELINE — Teensy 4.1 (600 MHz, 1 MB RAM, built in SD slot, no onboard radio), programmed in C++ |
| PLT-2 | Software shall be C++17, buildable with a standard CMake/g++ toolchain on a development machine. The Teensy toolchain is also C++, so the sampling/logging logic ports rather than gets rewritten. | BASELINE |
| PLT-3 | Sensor and storage access shall be isolated behind small interfaces (`ISensor`, file writer) so the simulated implementations used now can be swapped for real drivers later without changing the sampling/logging logic. | BASELINE |
| PLT-4 | The sampling loop shall hold 800 Hz without the scheduler itself becoming the limiting factor. | BASELINE — measured 798.7 Hz on a development host; the Teensy's microsecond timer should do better |
| PLT-5 | Sensors shall be split across separate buses so one hung device cannot stall the others. | BASELINE in the proposed pin plan (docs/HARDWARE.md); needs schematic confirmation |

## 7. Post Processing

| ID | Requirement | Status |
|----|-------------|--------|
| PST-1 | A script or program shall open a saved data file and produce basic plots of every logged stream over time. | BASELINE |
| PST-2 | The plotting tool shall be usable for post flight data visualization without requiring the onboard toolchain. | BASELINE |
| PST-3 | Python (matplotlib) is used for the reference plotting script; MATLAB is an acceptable alternative per the assignment and may be added later. | ASSUMED |
| PST-4 | The plotting tool shall report each stream's measured sample rate, so a rate that silently degraded in flight is visible. | BASELINE |

## 8. Deliverables (end of summer)

| ID | Requirement | Status |
|----|-------------|--------|
| DEL-1 | A repository for Payload Software. | BASELINE |
| DEL-2 | An initial data collection program using simulated inputs. | BASELINE |
| DEL-3 | A sample saved data file. | BASELINE |
| DEL-4 | A short README explaining how to run the software. | BASELINE |
| DEL-5 | Three recommended first steps for the fall semester. | BASELINE |

## 9. Flight State Detection

Added for the parachute reefing PDR. Full design in
`docs/FLIGHT_STATE_ALGORITHM.md`. This turns the existing sensor suite from
pure logging into something that also knows what phase of flight it is in,
which both answers CTL-3 and hands the reefing experiment its start signal.

| ID | Requirement | Status |
|----|-------------|--------|
| FSD-1 | The system shall derive altitude above ground level (AGL) from the pressure sensor, referenced to a ground pressure captured while idle on the pad. | BASELINE |
| FSD-2 | The system shall compute a filtered vertical velocity from successive altitude samples, for use in apogee and landing detection. | BASELINE |
| FSD-3 | The system shall detect apogee as the point where vertical velocity crosses from positive to negative, confirmed over a minimum number of consecutive samples so sensor noise cannot trigger it early. | BASELINE |
| FSD-4 | The system shall detect landing as altitude AGL settling within a small tolerance band together with low vibration, sustained for a minimum duration, before closing the data file. | BASELINE |
| FSD-5 | Flight states shall be detected from barometric data. The BMP390 is the payload's only altitude source by decision — no second barometer and no GPS — and if that path is faulted the reefing stages fall through to their per stage backup timers (REEF-4) rather than to a second measurement. | BASELINE — decided; single barometer accepted with timers as the failsafe |
| FSD-6 | Flight state detection shall be evaluated as the answer to CTL-3: launch detect starts recording and landed detect stops it, replacing the ground/dev Ctrl+C stand in (CTL-4) once validated on the bench and in a drop test. | TBD |

## 10. Parachute Reefing Experiment

Added for the parachute reefing PDR. Full design in
`docs/REEFING_ALGORITHM.md`. Assumes a dual deploy sequence — drogue at
apogee, main deployed in a reefed configuration at altitude, then stepped
open in stages — per the PDR discussion.

| ID | Requirement | Status |
|----|-------------|--------|
| REEF-1 | The main parachute shall deploy in a reefed configuration at a defined altitude AGL, after drogue deployment and confirmed descent under drogue. Deployment itself is commanded by the recovery system, not by this payload (REEF-8); the payload senses that it happened. | ASSUMED — trigger altitude pending flight simulation |
| REEF-2 | The main canopy shall open in three stages: initial reefed, one intermediate partial disreef, and full open, driven by a reefing **motor** unwinding a spool in two commanded steps. Not line cutters — one actuator, two commanded positions. | BASELINE |
| REEF-3 | Each disreef stage shall trigger on an altitude AGL threshold, confirmed over a minimum number of consecutive, descending samples so a noise spike cannot fire it early. | BASELINE |
| REEF-4 | Each stage shall carry a backup timer that fires it if the altitude trigger has not occurred within an expected window, so a barometric fault cannot leave the canopy reefed all the way to the ground. | ASSUMED |
| REEF-5 | Stages shall fire in order and latch: a later stage shall not be evaluated until the prior stage has fired, and no stage shall fire more than once. | BASELINE |
| REEF-6 | Every motor triggered stage change, whether by altitude trigger or backup timer, shall be timestamped on the same shared clock as the sensor data and logged. | TBD — pending CSV schema extension, see docs/DATA_FORMAT.md |
| REEF-7 | The altitude thresholds for reefed deploy and each disreef stage, and the reef percentage the mechanical hardware achieves at each stage, shall be confirmed with Structures/Recovery and the flight simulation before PDR sign off. | TBD |
| REEF-8 | The payload shall **not** command primary drogue or main deployment. It senses that a main deployment has occurred and runs the disreef stages from there; primary deployment stays with the dedicated recovery system. | BASELINE — decided |

## Open items to bring to Payload Electrical

1. **Flight simulation.** Not available yet, and it blocks the most: the reefed deploy altitude and both disreef altitudes (REEF-1, REEF-7), the per stage backup timer windows (REEF-4), and the debounce counts in both algorithm docs all stay ASSUMED placeholders until it exists. Nothing about the trigger thresholds can be frozen before then.
2. **Reefing motor, gearbox, and spool selection** (REEF-7), and how the target position is confirmed — encoder, limit switch, or stall current. This decides whether `MOTOR_FAULT` can be genuinely detected or only inferred from a missing confirmation.
3. How recording starts and stops in flight (CTL-3), now proposed to be answered by flight state detection (FSD-6) — launch detect starts the log, landed detect closes it.
4. How main deployment is *sensed*, given the payload no longer commands it (REEF-8). An altitude threshold alone does not confirm the canopy actually opened; see the open item in `docs/FLIGHT_STATE_ALGORITHM.md`.
5. Whether all three ADXL375 axes are logged, which triples the vibration data rate (SEN-12).
6. Confirmation of the proposed bus/pin assignment against the real schematic (PLT-5), including the two separate I2C buses.
7. Battery and regulator sizing once real power draw is measured on the breadboard (ERR-6).

## Decisions closed

| Was open | Decision |
|---|---|
| GPS / position logging (SEN-9, SEN-11) | Receiver removed from the payload; no position stream. |
| Altitude redundancy after the GPS removal (FSD-5) | Single barometer accepted. Flight states are detected from barometric data, every disreef trigger is an altitude threshold, and the per stage backup timers (REEF-4) are the failsafe. No second barometer. |
| Pressure sensor part (SEN-7) | BMP390 on I2C0, replacing the MS5611. SHT40 moves to I2C1 so it cannot stall the barometer. |
| Reefing actuation (REEF-2) | Motor driven spool, two commanded unwind positions. Not line cutters. |
| Primary deployment ownership (REEF-8) | The payload does not command drogue or main deployment. It senses that main deployment happened and runs the disreef stages from there. |
