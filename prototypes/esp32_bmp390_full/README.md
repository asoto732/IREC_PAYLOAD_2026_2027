# ESP32 + BMP390 Logger (full version, stretch)

> Stretch goal. The one day project is `../esp32_bmp390`. This version
> splits the program into modules and adds the stretch requirements
> (numbered files, BOOT button stop, fault counting, file commands).
> Its requirement IDs are E32F, in this folder's REQUIREMENTS.md.

Reads pressure and temperature from a BMP390 on a Freenove ESP32 WROVER and
saves them to `/log_NNN.txt` in the ESP32's internal flash (LittleFS).

**Requirements:** `REQUIREMENTS.md` in this folder. Every `TODO` in the
code is tagged with the requirement it satisfies, e.g. `TODO(E32F-17)`.

**Library:** [Adafruit BMP3XX](https://github.com/adafruit/Adafruit_BMP3XX),
installed automatically from `platformio.ini`.

## Wiring

| BMP390 breakout | ESP32 WROVER |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SCK (SCL) | GPIO 14 |
| SDI (SDA) | GPIO 13 |
| SDO, CS | leave unconnected (I2C, address 0x77) |

## Commands

```
pio run -t upload       # build and flash
pio device monitor      # open the serial monitor at 115200
```

In the monitor: `l` list files, `d` dump the newest log, `e` erase logs
(confirm with `Y`), `x` stop recording. The BOOT button also stops
recording.

To keep a dump as a file on your laptop, copy everything between
`BEGIN FILE` and `END FILE` into a `.txt`, or uncomment the `log2file`
line in `platformio.ini` so the monitor saves its output under `logs/`.

## Files

| File | What's in it | Your work |
|---|---|---|
| `include/Config.hpp` | Every pin, rate, limit and setting | Choose and justify the 3 sensor settings (E32F-8) |
| `src/Status.hpp` | Status codes and the `Reading` struct | Provided |
| `src/Barometer.cpp` | Talks to the Adafruit library | `begin()` and `read()` |
| `src/DataLogger.cpp` | The `.txt` file in LittleFS | Mount, file naming, writing, flushing, free space |
| `src/Commands.cpp` | Serial commands | Find newest log, dump, erase (`l` is a worked example) |
| `src/main.cpp` | Startup and the 10 Hz loop | Startup order, stop conditions, timing, writing |

## Suggested order

1. **Barometer.** Fill in `Barometer.cpp` and `setup()` up to
   `barometer.begin()`. Temporarily print one reading per second over Serial
   to prove the sensor works. Remove the print afterward (E32F-28).
2. **Logger.** Fill in `DataLogger.cpp`, then the rest of `setup()` and
   `loop()`. Record, stop with `x`, and check with `l` that a file appeared.
3. **Commands.** Fill in `Commands.cpp` so `d` dumps the file.
4. **Check.** Work through the "Done when" list at the bottom of
   `REQUIREMENTS.md`, including plotting the file with
   `python scripts/plot_data.py log_000.txt` from the repo root.

Splitting the work: one person on step 1, one on steps 2 and 3, then
everyone on step 4.
