# Payload Software

Data collection framework for the payload's pressure, temperature, and
vibration sensors. This is the summer year 2 initial version: sensors are
simulated in software because final hardware has not been selected by
Payload Electrical yet. See `docs/REQUIREMENTS.md` for the full,
requirement-centric specification this repo is built against, and
`docs/DATA_FORMAT.md` for the saved-data format.

## What's here

```
payload-software/
├── CMakeLists.txt          # build entry point
├── include/payload/        # headers: sensor interface, types, config
├── src/                    # main program + DataLogger implementation
├── tests/                  # minimal smoke test for the CSV logger
├── scripts/plot_data.py    # post-flight plotting tool (Python)
├── data/                   # sample_payload_log.csv + sample_plot.png
└── docs/                   # requirements and data format docs
```

## Requirements to build

- A C++17 compiler (developed with g++ 13; any recent g++/clang works)
- CMake 3.10+
- Python 3 with `matplotlib` (only needed for the plotting script; see
  `scripts/requirements.txt`)

## Building and running the data collection program

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/payload_collector
```

This starts a recording session immediately: it samples the simulated
pressure and temperature sensors at 1 Hz and the simulated vibration sensor
at 100 Hz (placeholders, see `docs/REQUIREMENTS.md` SEN-4/SEN-5), timestamps
every reading, and writes one CSV row per reading to a new, uniquely named
file under `data/` (e.g. `data/payload_log_20260824T180854Z.csv`).

Recording stops, and the file is safely flushed and closed, when any of the
following happens:

- You press `Ctrl+C`.
- The configured max run time elapses (30 s by default, `kMaxRunSeconds` in
  `include/payload/Config.hpp`; set to `0` to run until interrupted).
- Every sensor has gone offline after repeated simulated faults.

Console output reports the data file path and a final sample/fault count,
for example:

```
Recording stopped. Samples written: 3038, faulted reads: 28
Data file: data/payload_log_20260824T180854Z.csv
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
```

This produces one stacked plot each for pressure, temperature, and
vibration over time, with non-`OK` status rows marked in red so faulted
readings are visible instead of hidden. A MATLAB script reading the same
CSV with `readtable()` would work as an alternative post-flight tool per
the assignment; only the Python version is implemented so far (PST-3 in
`docs/REQUIREMENTS.md`).

## Sample data

`data/sample_payload_log.csv` is a real 30-second run of the program
included as the assignment's required sample saved data file.
`data/sample_plot.png` is the plot produced from it by `plot_data.py`.

## Design notes

- Sensors implement a small `ISensor` interface (`include/payload/ISensor.hpp`)
  so the simulated sensors used now can be swapped for real drivers later
  without touching the sampling loop or the CSV writer (PLT-3).
- The CSV format is one row per reading (not one row per timestamp) because
  vibration samples much faster than pressure/temperature; see
  `docs/DATA_FORMAT.md` for why.
- Every row carries a `status` column (`OK`, `TIMEOUT`, `OUT_OF_RANGE`,
  `COMM_ERROR`, `SENSOR_OFFLINE`, `STORAGE_ERROR`) so faulted readings are
  recorded, not silently dropped.
- If one sensor goes offline, the program keeps logging the remaining
  sensors instead of stopping entirely.

## Known placeholders (see docs/REQUIREMENTS.md for the full list)

Sampling rates, physical units/bounds, the storage location, and the
start/stop trigger are all software stand-ins until Payload Electrical
finalizes the microcontroller, sensors, and storage method. They are each a
single named constant so updating them later is a small, localized change.

## Fall semester next steps

See `docs/NEXT_STEPS.md`.
