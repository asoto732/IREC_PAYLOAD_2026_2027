# Payload Software Requirements

Status: preliminary, summer year 2. This list is the working baseline for the
data collection framework. It grows or shrinks as scope changes, and every
task in the repo should trace back to at least one requirement below.

Legend for status: `BASELINE` = decided and implemented now, `TBD` = open,
pending coordination with Payload Electrical, `ASSUMED` = a placeholder value
picked so development can proceed, expected to be revisited.

## 1. Sensing

| ID | Requirement | Status |
|----|-------------|--------|
| SEN-1 | The system shall read a pressure sensor at a defined sampling rate. | BASELINE |
| SEN-2 | The system shall read a temperature sensor at a defined sampling rate. | BASELINE |
| SEN-3 | The system shall read a vibration sensor at a defined sampling rate, independent of and normally faster than the pressure and temperature rate. | BASELINE |
| SEN-4 | Pressure and temperature shall sample at 1 Hz (once per second). | ASSUMED, pending electrical sensor selection |
| SEN-5 | Vibration shall sample at 100 Hz. | ASSUMED, pending electrical sensor selection and structural/vibration analysis needs |
| SEN-6 | Each sensor's sampling rate shall be a single named constant, changeable without touching unrelated code. | BASELINE |
| SEN-7 | The physical sensor models, communication interfaces (e.g. I2C, SPI, UART, analog), and part numbers shall be finalized with Payload Electrical. | TBD |

## 2. Timestamping and Data Format

| ID | Requirement | Status |
|----|-------------|--------|
| FMT-1 | Every measurement shall be timestamped. | BASELINE |
| FMT-2 | Timestamps shall be elapsed milliseconds since recording start, monotonic and not subject to wall clock adjustment. | ASSUMED |
| FMT-3 | Saved data shall include, at minimum: time, pressure, temperature, vibration, and a sensor/storage status or error indicator. | BASELINE |
| FMT-4 | Data shall be stored in CSV format with a header row naming each column. | BASELINE |
| FMT-5 | Because vibration samples faster than pressure/temperature, the CSV shall carry a `sensor` column identifying which reading a row belongs to, rather than forcing all three onto one fixed-width row. | BASELINE, see docs/DATA_FORMAT.md |

## 3. Storage

| ID | Requirement | Status |
|----|-------------|--------|
| STO-1 | Data shall be saved to the onboard storage method selected by the system integration team. | TBD |
| STO-2 | Until onboard storage is selected, the program shall write to a local filesystem path (SD card mount point or equivalent) as a stand in. | ASSUMED |
| STO-3 | The program shall create a new, uniquely named data file per recording session so prior sessions are never overwritten. | BASELINE |
| STO-4 | Available onboard memory and write throughput limits shall be confirmed with Payload Electrical so buffering/flush strategy can be sized correctly. | TBD |

## 4. Error Handling and Fault Tolerance

| ID | Requirement | Status |
|----|-------------|--------|
| ERR-1 | The system shall detect a sensor read failure (timeout, out of range value, or communication error) and record it rather than silently dropping data. | BASELINE |
| ERR-2 | The system shall detect a storage write failure and report it. | BASELINE |
| ERR-3 | Each row shall carry a per row status/error code, so post flight analysis can distinguish good data from faulted data without losing the row. | BASELINE |
| ERR-4 | If a sensor stops responding, the system shall continue collecting from the remaining sensors rather than halting the whole program. | BASELINE |
| ERR-5 | If a sensor stops responding or the program is asked to stop, the system shall safely close (flush and close) the data file so it is never left corrupt or truncated mid write. | BASELINE |

## 5. Recording Control

| ID | Requirement | Status |
|----|-------------|--------|
| CTL-1 | The system shall support an explicit start-of-recording event. | BASELINE |
| CTL-2 | The system shall support an explicit stop-of-recording event that safely closes the data file. | BASELINE |
| CTL-3 | How recording is triggered in flight (ground command, power on, deployment switch, timer) shall be defined with Payload Electrical/systems. | TBD |
| CTL-4 | Until CTL-3 is resolved, the ground/dev version shall start on program launch and stop on a keyboard interrupt (Ctrl+C) or a fixed run duration, both of which invoke the same safe shutdown path. | ASSUMED |

## 6. Platform

| ID | Requirement | Status |
|----|-------------|--------|
| PLT-1 | The onboard microcontroller/SBC and its programming language shall be selected jointly with Payload Electrical. | TBD |
| PLT-2 | Until PLT-1 is resolved, software shall be developed in C++17, buildable with a standard CMake/g++ toolchain, since it is portable to the largest range of likely targets (bare metal MCU via a HAL, or an embedded Linux SBC). | ASSUMED |
| PLT-3 | Sensor and storage access shall be isolated behind small interfaces (`ISensor`, file writer) so the simulated implementations used now can be swapped for real drivers later without changing the sampling/logging logic. | BASELINE |

## 7. Post Processing

| ID | Requirement | Status |
|----|-------------|--------|
| PST-1 | A script or program shall open a saved data file and produce basic plots of pressure, temperature, and vibration over time. | BASELINE |
| PST-2 | The plotting tool shall be usable for post flight data visualization without requiring the onboard toolchain. | BASELINE |
| PST-3 | Python (matplotlib/pandas) is used for the reference plotting script; MATLAB is an acceptable alternative per the assignment and may be added later. | ASSUMED |

## 8. Deliverables (end of summer)

| ID | Requirement | Status |
|----|-------------|--------|
| DEL-1 | A repository for Payload Software. | BASELINE |
| DEL-2 | An initial data collection program using simulated inputs. | BASELINE |
| DEL-3 | A sample saved data file. | BASELINE |
| DEL-4 | A short README explaining how to run the software. | BASELINE |
| DEL-5 | Three recommended first steps for the fall semester. | BASELINE |

## Open items to bring to Payload Electrical

1. Final sensor part numbers and their native communication interfaces (SEN-7).
2. Confirmed sampling rates for pressure, temperature, and vibration (SEN-4, SEN-5).
3. Onboard storage method and available memory/write budget (STO-1, STO-4).
4. Target microcontroller/SBC and resulting programming language (PLT-1).
5. How recording starts and stops in flight (CTL-3).
