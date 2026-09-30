# ESP32 + BMP390 Logger: Requirements

**Objective:** read temperature and pressure from a BMP390 on a Freenove
ESP32 WROVER and save the readings to a `.txt` file in the ESP32's flash.
Sized to finish in about a day.

Every line of code you write must satisfy one of the requirements below.
Each one traces to a flight requirement in `docs/REQUIREMENTS.md` or
`docs/SENSOR_REQUIREMENTS.md`, because this project rehearses the flight
software's core job: read, timestamp, save, and close the file safely.

Verification: **I** = inspection / code review, **T** = test on the board,
**D** = demo to the team.

## Core requirements

| ID | Requirement | Traces to | Verify |
|---|---|---|---|
| E32-1 | The project shall build in PlatformIO with `platform = espressif32`, `board = esp32dev`, `framework = arduino`, with no compiler warnings. | PLT-2 | I |
| E32-2 | The BMP390 shall connect over I2C: VIN to 3V3, GND to GND, SDA to GPIO 13, SCL to GPIO 14, address 0x77. The wiring shall be drawn as a paper schematic before it is built. | SEN-7, PLT-5 | I |
| E32-3 | The sensor shall be read with the Adafruit BMP3XX Library. If it is not found at startup, the program shall print a message and retry every second instead of continuing. | DRV-3, DRV-12 | T |
| E32-4 | The program shall mount LittleFS and print a message if mounting fails. | STO-1, ERR-2 | T |
| E32-5 | Pressure and temperature shall be read at 10 Hz, timed with `micros()`. The loop shall not use `delay()`. | SEN-4, PLT-4 | I, T |
| E32-6 | Pressure shall be saved in kPa and temperature in °C. | FMT-3 | T |
| E32-7 | Readings shall be saved to `/log.txt` in LittleFS. Each run replaces the previous file. | STO-1 | T |
| E32-8 | The file shall start with the header `time_us,sensor,value,status` and hold one line per reading, in the same format as the flight logger (`docs/DATA_FORMAT.md`). `time_us` is microseconds since recording started. | FMT-1, FMT-2, FMT-4, FMT-5 | T |
| E32-9 | A failed read shall still be written, as `COMM_ERROR` lines, and the program shall keep running. | ERR-1, ERR-3, ERR-4 | T |
| E32-10 | Recording shall stop after 60 seconds, and the file shall be flushed and closed. | CTL-2, ERR-5 | T |
| E32-11 | The file shall be flushed to flash at least once per second. | ERR-6 | I |
| E32-12 | Typing `d` in the serial monitor after recording shall print the file between `BEGIN FILE` and `END FILE` lines. *(Provided in the skeleton.)* | PST-2 | T |
| E32-13 | The saved file shall plot with `scripts/plot_data.py` with no changes, and the reported rate shall be about 10 Hz. | PST-1, PST-4 | D |

## Stretch requirements

Not needed for this project. They are implemented as TODOs in
`prototypes/esp32_bmp390_full` (as E32F IDs) for anyone who finishes early.

| ID | Requirement | Traces to |
|---|---|---|
| S-1 | A new, uniquely numbered file per session, so old runs are never overwritten. | STO-3 |
| S-2 | Stop recording cleanly when flash is 90 % full. | STO-4 |
| S-3 | Stop on the BOOT button or on `x` typed in the monitor, not only on a timer. | CTL-2 |
| S-4 | After 5 failed reads in a row, log `SENSOR_OFFLINE` and close the file. | ERR-4, DRV-7 |
| S-5 | Flag readings outside the datasheet range as `OUT_OF_RANGE`. | SEN-10 |
| S-6 | Serial commands to list and erase log files. | PST-2 |
| S-7 | Choose the oversampling and filter settings from the datasheet and justify them. | BMP-5, BMP-6 |
| S-8 | Split the program into sensor, logger and command modules. | PLT-3 |
