# Sensor Driver Requirements

Status: DRAFT for the fall driver work. One team member owns each sensor and
writes its driver in PlatformIO for the Teensy 4.1. Parent requirements live
in `docs/REQUIREMENTS.md`; every requirement below traces to at least one of
them. Part choices and pins come from `docs/HARDWARE.md`.

Status legend (same as `docs/REQUIREMENTS.md`): `BASELINE` = decided,
`ASSUMED` = placeholder so work can proceed, `TBD` = open.

Verification legend:

| Code | Method | Can be done without hardware? |
|---|---|---|
| **C** | Compiles in the `teensy41` PlatformIO environment with no warnings | Yes |
| **H** | Host unit test (runs on a laptop, no Teensy) | Yes |
| **I** | Inspection / code review | Yes |
| **B** | Bench test on the real part | No, waits for hardware |

## Assignments

| Sensor | Streams it feeds | Bus | Owner |
|---|---|---|---|
| BMP390 | `PRESSURE`, `TEMPERATURE` | I2C0 (`Wire`) | |
| ADXL375 | `VIBRATION` | SPI0 | |
| SHT40 | `HUMIDITY` | I2C1 (`Wire1`) | |

## 1. Common requirements (every driver)

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| DRV-1 | Each driver shall implement the existing `payload::ISensor` interface (`include/payload/ISensor.hpp`) with no changes to that interface, so it drops into the sampling loop in place of its simulated class. | PLT-3 | BASELINE | C, I |
| DRV-2 | Each driver shall live in its own `.hpp`/`.cpp` pair named after the part (`Bmp390.hpp`, `Adxl375.hpp`, `Sht40.hpp`) and take its bus object (`TwoWire&` or `SPIClass&`) and pin/address settings through its constructor, not hard coded inside the class. | PLT-5, SEN-6 | BASELINE | I |
| DRV-3 | Each driver shall provide a `begin()` that verifies the part's identity (chip ID or serial number) and applies its configuration. `begin()` shall return failure instead of hanging if the part is absent. | ERR-1 | BASELINE | H, B |
| DRV-4 | `read()` shall never call `delay()` or wait on a conversion. It shall return within the per part time budget below, so one slow sensor cannot stall the 800 Hz loop. | PLT-4, ERR-4 | BASELINE | I, B |
| DRV-5 | A bus that does not respond within `kBusTimeoutUs` shall produce `StatusCode::Timeout`, not a hang. | ERR-1, PLT-5 | ASSUMED (`kBusTimeoutUs` = 1000) | B |
| DRV-6 | Failures shall map to the existing status codes: no response = `Timeout`, bad ID / bad CRC / bus error = `CommError`, value outside the datasheet range = `OutOfRange` (value still reported). | ERR-1, ERR-3 | BASELINE | H |
| DRV-7 | After `kOfflineAfterConsecutiveFaults` consecutive faults the driver shall report `isOffline() == true` and return `SensorOffline`. One good read resets the count. Behavior shall match `SimulatedSensorBase`. | ERR-4 | BASELINE | H |
| DRV-8 | Range limits shall come from the existing constants in `Config.hpp`, not new literals. Any new tunable (ODR, oversampling, filter, clock speed) shall be added there as one named constant. | SEN-6, SEN-10 | BASELINE | I |
| DRV-9 | Values shall be returned in logged units: kPa, °C, g, %RH. | FMT-3, DATA_FORMAT.md | BASELINE | H |
| DRV-10 | Conversion from raw register bytes to engineering units shall be a pure function with no bus access, so it can be unit tested on a laptop. | PLT-2, PLT-3 | BASELINE | H |
| DRV-11 | `read()` shall not allocate memory or print to `Serial`. Debug printing, if any, sits behind one compile time flag that is off by default. | PLT-4 | BASELINE | I |
| DRV-12 | A vendor library (e.g. Adafruit) may be used only if the finished driver still meets DRV-4 and DRV-5. Otherwise the driver talks to registers directly. | PLT-4 | BASELINE | I |
| DRV-13 | Each driver shall come with a small test sketch that runs `begin()` and prints readings over `Serial`, for bench day. | NEXT_STEPS step 1 | BASELINE | C, B |

**Per call time budget** (800 Hz tick = 1250 µs, shared by every sensor plus
the SD write):

| Part | Max time per `read()` | Status |
|---|---|---|
| ADXL375 | 100 µs | ASSUMED |
| BMP390 | 400 µs | ASSUMED |
| SHT40 | 400 µs | ASSUMED |

## 2. BMP390: pressure and temperature

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| BMP-1 | The driver shall talk to the BMP390 on `Wire` (I2C0, pins 18 SDA / 19 SCL) at 400 kHz. The I2C address shall be a constant, default 0x77 (Adafruit breakout default; 0x76 if SDO is tied low). | SEN-7, PLT-5 | BASELINE (pins need schematic confirmation) | C, B |
| BMP-2 | `begin()` shall read the chip ID register and accept only 0x60 (BMP390). 0x50 means a BMP388 and shall be rejected. | DRV-3 | BASELINE | H, B |
| BMP-3 | `begin()` shall read the factory calibration coefficients once and store them. Compensation shall follow the datasheet's floating point formulas. | SEN-1, SEN-2 | BASELINE | H |
| BMP-4 | The part shall run in **normal mode** at a 100 Hz output data rate, so `read()` only fetches the latest result and never triggers or waits on a conversion (forced mode is not allowed). | SEN-4, DRV-4 | BASELINE | I, B |
| BMP-5 | Oversampling shall be chosen so one conversion fits inside 10 ms. By the datasheet's timing formula, pressure oversampling above x2 (with temperature x1) cannot hold 100 Hz. Chosen settings and the resulting conversion time shall be written down in `Config.hpp`. | SEN-4, SEN-6 | ASSUMED (pressure x2, temperature x1) | I, B |
| BMP-6 | The IIR filter coefficient shall be a named constant. Because filtering adds lag to the only altitude source, the value shall be agreed with whoever owns the flight state algorithm before flight. | FSD-2, FSD-3 | TBD (start with filter off) | I |
| BMP-7 | Pressure and temperature shall come from **one** burst read of the data registers per 10 ms period. The driver shall expose two `ISensor` objects (one `Pressure`, one `Temperature`) that share that single read, so the pair costs one bus transaction and stays time aligned. | SEN-1, SEN-2, FMT-6 | BASELINE | H, I |
| BMP-8 | The driver shall check the status register's data ready flags and return `Timeout` if no new data has arrived since the last read, rather than logging a stale value twice. | ERR-1, PST-4 | BASELINE | B |
| BMP-9 | If the chip faults, **both** streams shall report the fault and both shall go offline together. | ERR-4, FSD-5 | BASELINE | H |
| BMP-10 | Pressure output shall keep at least 0.1 Pa resolution through the conversion (no float truncation before logging). | FMT-7 | BASELINE | H |

## 3. ADXL375: vibration

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| ADX-1 | The driver shall talk to the ADXL375 on SPI0 (11 MOSI / 12 MISO / 13 SCK, CS pin 10) in SPI mode 3 at no more than 5 MHz. CS pin and clock shall be constants. | SEN-7, PLT-5 | BASELINE (pins need schematic confirmation) | C, B |
| ADX-2 | `begin()` shall read the device ID register and accept only 0xE5. | DRV-3 | BASELINE | H, B |
| ADX-3 | The output data rate shall be set at or above the 800 Hz sampling rate. Whether to run the part at 800 Hz or 1600 Hz ODR (which changes the internal bandwidth and aliasing) shall be decided from the datasheet and recorded in `Config.hpp`. | SEN-5, design notes on aliasing | TBD | I |
| ADX-4 | All three axes shall be read in one multibyte burst (6 data bytes) every sample, even while only one axis is logged, so moving to three channels later (SEN-12) needs no driver change. | SEN-12 | BASELINE | I |
| ADX-5 | Which axis is logged as `VIBRATION` shall be one named constant. Default: the axis aligned with the rocket's long axis once mounting is known. | SEN-12, SEN-6 | ASSUMED | I |
| ADX-6 | Raw counts shall be converted to g at the datasheet scale factor (49 mg/LSB typical, 16 bit two's complement, little endian). | SEN-10, DRV-9 | BASELINE | H |
| ADX-7 | A reading at or beyond ±200 g shall be logged as `OutOfRange` with the value kept, so clipping at ignition or deployment is visible. | SEN-10, ERR-1 | BASELINE | H |
| ADX-8 | A zero g offset shall be measured on the bench and stored as a constant. It shall not be auto calibrated on the pad, since the pad is not guaranteed to be still. | SEN-5 | TBD | B |
| ADX-9 | Use of the part's 32 sample FIFO to absorb loop jitter shall be evaluated. If adopted, each FIFO sample still gets its own timestamp on the shared clock. | PLT-4, FMT-6 | TBD | B |

## 4. SHT40: humidity

| ID | Requirement | Traces to | Status | Verify |
|---|---|---|---|---|
| SHT-1 | The driver shall talk to the SHT40 on `Wire1` (I2C1, pins 17 SDA1 / 16 SCL1), on a separate bus from the BMP390, at 400 kHz. Address 0x44. | SEN-7, PLT-5 | BASELINE (pins need schematic confirmation) | C, B |
| SHT-2 | `begin()` shall send a soft reset and read the serial number to confirm the part is present. | DRV-3 | BASELINE | B |
| SHT-3 | Measurement shall be split into **trigger** and **fetch**: each 1 s period, one call sends the high precision measure command and a later call (at least the datasheet's max measurement time, ≈8.3 ms, afterward) reads the result. `read()` never waits for the conversion. The Adafruit library's blocking read does not meet this. | DRV-4, DRV-12 | BASELINE | I, B |
| SHT-4 | Each 16 bit word received shall be checked with the SHT4x CRC 8 (polynomial 0x31, init 0xFF). A CRC mismatch shall produce `CommError`. | ERR-1 | BASELINE | H |
| SHT-5 | Humidity shall be converted as RH = −6 + 125 × raw / 65535 and clamped to 0 to 100 %RH, per the datasheet. Out of range before clamping shall still be flagged. | SEN-8, DRV-9 | BASELINE | H |
| SHT-6 | The heater shall stay off. | SEN-8 | BASELINE | I |
| SHT-7 | Whether the SHT40's own temperature channel is logged as a second temperature stream (a cross check on the BMP390) shall be decided. | SEN-2 | TBD | — |
| SHT-8 | A hung or missing SHT40 shall have no effect on BMP390 reads. | PLT-5, FSD-5 | BASELINE | B |

## 5. What "done tonight" means (no hardware)

Each owner delivers:

1. Their driver `.hpp`/`.cpp` implementing `ISensor`, compiling in the
   `teensy41` environment (**C**).
2. The pure conversion function(s) with host unit tests (**H**): for the
   BMP390, compare against the Bosch reference compensation for the same raw
   inputs; for the ADXL375, known counts to g including ±200 g limits; for
   the SHT40, known raw words to %RH plus a CRC pass and a CRC fail case.
3. The fault counting and status mapping tests (DRV-6, DRV-7) run on a
   laptop with a fake bus that returns errors on demand.
4. A bench sketch (DRV-13) ready for the day the parts arrive.

Everything marked **B** stays open until the breadboard bring up in
`docs/NEXT_STEPS.md` step 1.

## Shared PlatformIO environment

Everyone uses the same environment so the drivers merge cleanly:

```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
build_flags = -Wall -Wextra -std=gnu++17
build_unflags = -std=gnu++14
```

Host tests can run under a separate `[env:native]` using PlatformIO's
built in Unity test runner, which only builds the pure conversion code.

## Open items raised by this draft

1. BMP-6: IIR filter setting, agreed with the flight state algorithm owner.
2. ADX-3: ADXL375 ODR (800 vs 1600 Hz) and its aliasing tradeoff.
3. ADX-5 / SEN-12: which axis, or all three.
4. SHT-7: log the SHT40 temperature or not.
5. DRV-5 and the time budgets: confirm on the bench.
