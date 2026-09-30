# BMP390 on a Freenove ESP32 WROVER (register level stretch version)

> Stretch goal. The main exercise is `../esp32_bmp390`, which uses the
> Adafruit library. This version writes the driver from the datasheet
> registers instead, with laptop tests.

A practice run of the flight BMP390 driver on a board we already have. The
driver written here is the real one: it implements the same `ISensor`
interface as the flight code and talks to the chip through a small `IBus`
interface, so moving to the Teensy 4.1 later means swapping `BoardConfig.hpp`
and `ArduinoI2cBus.hpp`, not rewriting the driver.

Requirements: `docs/SENSOR_REQUIREMENTS.md` (DRV, BMP) in the main repo.
Every `TODO` in the code is tagged with the requirement it satisfies.

## Wiring

| BMP390 breakout | Freenove ESP32 WROVER |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SCK (SCL) | GPIO 14 |
| SDI (SDA) | GPIO 13 |
| SDO | leave unconnected (address 0x77) |
| CS | leave unconnected (keeps it in I2C mode) |

The Adafruit breakout has its own pullups, so no extra resistors are needed.
Pins live in `include/BoardConfig.hpp` if you need to move them.

## Commands

```
pio test -e native            # host tests, no board needed
pio run -e esp32 -t upload    # build and flash
pio device monitor            # watch the CSV output at 115200 baud
```

The native tests need a C++ compiler on your laptop (MinGW g++ on Windows).

## What's provided and what you fill in

| File | Status |
|---|---|
| `lib/payload_common/` | Provided. Copies of the flight `ISensor` and `Types`. Do not edit. |
| `lib/bmp390/src/IBus.hpp` | Provided. |
| `lib/bmp390/src/Bmp390Regs.hpp` | Provided, but **check every value against the datasheet.** |
| `lib/bmp390/src/Bmp390Config.hpp` | Provided. One TODO: write down the conversion time (BMP-5). |
| `lib/bmp390/src/Bmp390Math.cpp` | **TODO.** Byte parsing, calibration, compensation. |
| `lib/bmp390/src/Bmp390.cpp` | **TODO.** `begin()` and `update()`. |
| `src/ArduinoI2cBus.hpp` | **TODO.** `readRegs()` (`writeReg()` is the worked example). |
| `src/main.cpp` | **TODO.** Start the bus, retry `begin()`, 100 Hz timing. |
| `test/` | Provided. Every test fails right now; your job is to make them pass. |

## Suggested order

1. **Read the datasheet** sections on the register map, calibration
   coefficients, and floating point compensation. Check `Bmp390Regs.hpp`.
2. **`Bmp390Math.cpp`** until `pio test -e native -f test_math` passes.
3. **`Bmp390.cpp`** until `pio test -e native -f test_driver` passes.
4. **`ArduinoI2cBus.hpp` and `main.cpp`**, then flash the board.

Steps 1 to 3 need no hardware.

## Done when

- [ ] All native tests pass.
- [ ] Builds for `esp32` with no warnings.
- [ ] On the board, the monitor shows `PRESSURE` and `TEMPERATURE` rows at
      100 Hz with status `OK`, and room pressure is about 101 kPa (lower if
      you are above sea level).
- [ ] No `# WARN update() took ...` lines (DRV-4 time budget).
- [ ] Unplugging SDA mid run gives `COMM_ERROR` or `TIMEOUT` rows, then
      `SENSOR_OFFLINE`, and the board does not freeze (DRV-5, DRV-7).
- [ ] One real sample (raw bytes plus calibration) is added as a test,
      checked against Bosch's reference BMP3 Sensor API.
