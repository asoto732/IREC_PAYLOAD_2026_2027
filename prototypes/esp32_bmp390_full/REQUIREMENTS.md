# ESP32 + BMP390 Logger: Requirements

**Objective:** read temperature and pressure from a BMP390 on a Freenove
ESP32 WROVER and save the readings to a `.txt` file in the ESP32's internal
flash.

This is a practice project on a board we already have. It rehearses the same
jobs the flight software does on the Teensy 4.1 (read, timestamp, save,
handle faults, close the file safely), so every requirement below traces to
a flight requirement in `docs/REQUIREMENTS.md` or `docs/SENSOR_REQUIREMENTS.md`.
Anything you code must satisfy at least one requirement here.

Status: `BASELINE` = decided, `ASSUMED` = placeholder, may change.

Verification: **I** = inspection / code review, **T** = test on the board,
**D** = demo to the team.

## 1. Hardware and tools

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| E32F-1 | The project shall be a PlatformIO project using `platform = espressif32`, `board = esp32dev`, `framework = arduino`. | PLT-2 | BASELINE | I |
| E32F-2 | The BMP390 shall connect over I2C: VIN to 3V3, GND to GND, SDA to GPIO 13, SCL to GPIO 14, address 0x77. Pins and address shall be named constants, not literals scattered through the code. | SEN-7, SEN-6 | BASELINE | I, T |
| E32F-3 | The sensor shall be driven with the **Adafruit BMP3XX Library** (`lib_deps = adafruit/Adafruit BMP3XX Library`), started with `begin_I2C(address, &Wire)`. Using `performReading()` is allowed for this exercise. | DRV-12 | BASELINE | I |
| E32F-4 | The filesystem shall be LittleFS (`board_build.filesystem = littlefs` in `platformio.ini`). | STO-1 | BASELINE | I |

## 2. Sensing

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| E32F-5 | The program shall read pressure and temperature at a fixed rate set by one named constant. | SEN-1, SEN-2, SEN-6 | BASELINE | I |
| E32F-6 | The sample rate shall be 10 Hz. | SEN-4 | ASSUMED (flight is 100 Hz; 10 Hz suits `performReading()`) | T |
| E32F-7 | Timing shall use `millis()` or `micros()` with a "next sample time" that advances by one period each sample. `delay()` shall not be used to set the rate. | PLT-4, DRV-4 | BASELINE | I |
| E32F-8 | Pressure oversampling, temperature oversampling and the IIR filter shall be set explicitly in `setup()` from named constants, not left at library defaults. | SEN-6, BMP-5, BMP-6 | BASELINE | I |
| E32F-9 | Pressure shall be saved in **kPa** and temperature in **°C**. The library returns pressure in Pa, so convert it. | DRV-9 | BASELINE | T |
| E32F-10 | A reading outside 30 to 125 kPa or −40 to 85 °C shall be saved with status `OUT_OF_RANGE`, keeping the value. | SEN-10, ERR-1 | BASELINE | I |

## 3. File format

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| E32F-11 | Data shall be saved as a plain text `.txt` file, one reading per line, comma separated. | FMT-4 | BASELINE | T |
| E32F-12 | The first line shall be the header `time_us,sensor,value,status`, the same format the flight logger uses (`docs/DATA_FORMAT.md`). Each sample produces two lines: one `PRESSURE`, one `TEMPERATURE`, with the same timestamp. | FMT-3, FMT-5 | BASELINE | T |
| E32F-13 | `time_us` shall be microseconds since recording started (not since boot). | FMT-1, FMT-2 | BASELINE | T |
| E32F-14 | Values shall be written with 7 significant digits (`%.7g`). | FMT-7 | BASELINE | I |
| E32F-15 | `status` shall be one of `OK`, `OUT_OF_RANGE`, `COMM_ERROR`, `SENSOR_OFFLINE`, `STORAGE_ERROR`. | ERR-3 | BASELINE | I |

Example:

```
time_us,sensor,value,status
0,PRESSURE,101.3251,OK
0,TEMPERATURE,23.41882,OK
100012,PRESSURE,101.3248,OK
100012,TEMPERATURE,23.42011,OK
```

## 4. Storage

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| E32F-16 | Each recording session shall create a **new** file (`/log_000.txt`, `/log_001.txt`, …), picking the next unused number, so earlier sessions are never overwritten. | STO-3 | BASELINE | T |
| E32F-17 | The file shall be flushed at least once per second, so a reset or power loss costs at most about one second of data. | ERR-6, STO-4 | BASELINE | T |
| E32F-18 | Before each write, free space shall be checked. Below 10 % free, recording shall stop and the file shall be closed cleanly. | STO-4, ERR-5 | BASELINE | T |
| E32F-19 | A failed LittleFS mount at boot shall be reported over Serial. The program shall not claim to be recording when it is not. | ERR-2 | BASELINE | T |
| E32F-20 | A failed write (the write call returns 0 bytes) shall be reported over Serial as `STORAGE_ERROR`. | ERR-2 | BASELINE | I |

## 5. Faults

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| E32F-21 | If the sensor is not found at boot, the program shall print a clear message (check wiring / address) and retry once per second. It shall not create a data file until the sensor responds. | DRV-3 | BASELINE | T |
| E32F-22 | A failed read during recording shall be saved as a `COMM_ERROR` line. The program keeps running. | ERR-1, ERR-4 | BASELINE | T |
| E32F-23 | After 5 failed reads in a row, the program shall write `SENSOR_OFFLINE`, close the file, and stop recording. One good read resets the count. | DRV-7, ERR-5 | BASELINE | T |

## 6. Start, stop and getting the file off the board

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| E32F-24 | Recording shall start automatically once the sensor and filesystem are both ready. | CTL-1, CTL-4 | BASELINE | T |
| E32F-25 | Recording shall stop, and the file shall be flushed and closed, on any of: the BOOT button (GPIO 0) pressed, `x` typed in the Serial monitor, or a max run time (named constant, default 60 s). | CTL-2, CTL-4, ERR-5 | BASELINE | T |
| E32F-26 | Serial commands: `l` lists files with sizes, `d` dumps the most recent file, `e` erases all log files after asking to confirm. | PST-2 | BASELINE | T |
| E32F-27 | A dump shall print `BEGIN FILE <name>` and `END FILE` lines around the contents so it can be cut out and saved as a `.txt` on a laptop. | PST-1 | BASELINE | T |
| E32F-28 | Serial output during recording shall be limited to status messages (start, stop, errors), not every reading, so printing does not slow sampling. | PLT-4 | BASELINE | I |

## 7. Done when

- [ ] Builds with no warnings.
- [ ] A 60 s recording at room conditions produces a `.txt` with the header
      and about 1,200 lines (600 samples × 2).
- [ ] Pressure reads about 101 kPa (lower above sea level) and temperature
      is close to room temperature.
- [ ] Blowing gently on the sensor visibly changes temperature in the file.
- [ ] The dumped file plots with the flight tool, with no changes to it:
      `python scripts/plot_data.py log_000.txt`. The script prints each
      stream's measured rate; it should show about 10 Hz (E32F-6).
- [ ] Pressing BOOT mid recording closes the file, and the file still plots.
- [ ] Unplugging SDA mid recording produces `COMM_ERROR` lines, then
      `SENSOR_OFFLINE`, and the board does not freeze.
- [ ] Rebooting creates `log_001.txt` and leaves `log_000.txt` intact.
