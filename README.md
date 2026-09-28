# Payload Software

Data collection framework for the payload's pressure, temperature,
vibration, and humidity sensors.

The flight hardware has now been selected by Payload Electronics — BMP390,
ADXL375, SHT40, on a Teensy 4.1 logging to microSD — so the
sampling rates, sensor ranges, and storage budget in this repo come from
those parts rather than from placeholders. The sensors themselves are still
simulated in software: the parts are chosen but the drivers are fall work.

- `docs/HARDWARE.md` — the selected parts, the proposed bus/pin plan, and
  what each choice means for software.
- `docs/REQUIREMENTS.md` — the requirement-centric specification this repo
  is built against.
- `docs/DATA_FORMAT.md` — the saved-data format.

## What's here

```
payload-software/
├── CMakeLists.txt          # build entry point
├── include/payload/        # headers: sensor interface, types, config
├── src/                    # main program + DataLogger implementation
├── tests/                  # minimal smoke test for the CSV logger
├── scripts/plot_data.py    # post-flight plotting tool (Python)
├── data/                   # sample_payload_log.csv + sample_plot.png
└── docs/                   # hardware, requirements, data format, next steps
```

## Requirements to build

- A C++17 compiler (developed with g++; builds with MinGW on Windows too)
- CMake 3.10+
- Python 3 with `matplotlib` (only needed for the plotting script; see
  `scripts/requirements.txt`)

## Building and running the data collection program

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/payload_collector
```

This starts a recording session immediately and samples each stream at the
rate its selected part supports:

| Stream | Part | Rate |
|---|---|---|
| Pressure | BMP390 | 100 Hz |
| Temperature | BMP390 | 100 Hz |
| Vibration | ADXL375 | 800 Hz |
| Humidity | SHT40 | 1 Hz |

Every reading is timestamped off one shared clock in microseconds and
written as one CSV row to a new, uniquely named file under `data/` (e.g.
`data/payload_log_20260914T225932Z.csv`). Rates and ranges all live in
`include/payload/Config.hpp`, one named constant each.

Recording stops, and the file is safely flushed and closed, when any of the
following happens:

- You press `Ctrl+C`.
- The configured max run time elapses (30 s by default, `kMaxRunSeconds` in
  `include/payload/Config.hpp`; set to `0` to run until interrupted).
- Every sensor has gone offline after repeated simulated faults.

Console output reports the data file path and a final sample/fault count,
for example:

```
Recording stopped. Samples written: 30890, faulted reads: 50
Data file: data/payload_log_20260914T225932Z.csv
```

## Running the tests

```bash
cd build
ctest --output-on-failure
```

## Plotting saved data

```bash
pip install -r scripts/requirements.txt   # once, if matplotlib isn't installed
python3 scripts/plot_data.py data/sample_payload_log.csv --out plot.png
# or, for an interactive window instead of a saved file:
python3 scripts/plot_data.py data/sample_payload_log.csv --show
# or just the streams you care about:
python3 scripts/plot_data.py data/sample_payload_log.csv --only PRESSURE VIBRATION
```

This produces one stacked plot per stream, with faulted readings marked in
red so they are visible instead of hidden. It also prints each stream's
**measured** sample rate, which is the quickest way to catch a rate that
silently degraded:

```
  PRESSURE       samples=  3000  measured rate=  100.0 Hz  non-OK status=2
  VIBRATION      samples= 23972  measured rate=  799.1 Hz  non-OK status=26
  HUMIDITY       samples=    30  measured rate=    1.0 Hz  non-OK status=0
```

A MATLAB script reading the same CSV with `readtable()` would work as an
alternative post-flight tool per the assignment; only the Python version is
implemented so far (PST-3 in `docs/REQUIREMENTS.md`).

## Sample data

`data/sample_payload_log.csv` is a real 30-second run of the program at the
flight sampling rates (30,002 rows, ≈956 KB — the basis for the storage
budget in `docs/HARDWARE.md`). `data/sample_plot.png` is the plot produced
from it by `plot_data.py`.

## Design notes

- Sensors implement a small `ISensor` interface (`include/payload/ISensor.hpp`)
  so the simulated sensors used now can be swapped for real drivers later
  without touching the sampling loop or the CSV writer (PLT-3).
- The CSV format is one row per reading (not one row per timestamp) because
  the streams sample at rates from 1 Hz to 800 Hz; see `docs/DATA_FORMAT.md`.
  Removing the GPS streams changed which rows appear, not the format.
- Timestamps are microseconds, not milliseconds: 800 Hz is a 1.25 ms period,
  which millisecond timestamps cannot represent.
- GPS was removed from the payload after the parts were selected, and the
  BMP390 replaced the MS5611 as the barometer. Barometric altitude is now
  the only altitude measurement on board — a settled decision, with the
  reefing backup timers as the failsafe (FSD-5).
- Out-of-range bounds come from the selected parts' datasheets — notably
  ±200 g for the ADXL375, which is why that part was picked over a ±16 g IMU.
- Rows are buffered and flushed every `kFlushIntervalMs` rather than per
  row; an SD write per sample is not affordable at 800 Hz, and slow SD
  writes are on the electronics risk list.
- Every row carries a `status` column (`OK`, `TIMEOUT`, `OUT_OF_RANGE`,
  `COMM_ERROR`, `SENSOR_OFFLINE`, `STORAGE_ERROR`) so faulted readings are
  recorded, not silently dropped.
- If one sensor goes offline, the program keeps logging the remaining
  sensors instead of stopping entirely.

## Still simulated / still open

- The sensors are simulated. The parts are selected; writing the real
  drivers is step 1 of the fall plan.
- The scheduler sleeps and then briefly spins to hold 800 Hz, because a
  desktop OS overshoots short sleeps by a full scheduler tick. The Teensy's
  microsecond timer will not need the spin.
- How recording starts and stops in flight (`CTL-3`) is the last blocking
  open item; the program currently starts on launch and stops on Ctrl+C.
- Whether all three ADXL375 axes get logged (`SEN-12`) is undecided.
- The flight state and reefing algorithms are designed but not implemented;
  see `docs/FLIGHT_STATE_ALGORITHM.md` and `docs/REEFING_ALGORITHM.md`.
  Their thresholds stay placeholders until a flight simulation exists.

## Fall semester next steps

See `docs/NEXT_STEPS.md`.
