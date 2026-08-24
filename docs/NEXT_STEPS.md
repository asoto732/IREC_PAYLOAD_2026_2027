# Recommended First Steps for Fall Semester

These follow directly from the open (`TBD`) items in `docs/REQUIREMENTS.md`
and are ordered by what blocks the most other work.

## 1. Close out the open requirements with Payload Electrical

Everything currently marked `TBD` in `docs/REQUIREMENTS.md` blocks a real
implementation decision:

- Final microcontroller/SBC and the language it forces (`PLT-1`).
- Sensor part numbers and their communication interfaces -- I2C, SPI, UART,
  or analog (`SEN-7`).
- Confirmed sampling rates for pressure, temperature, and vibration
  (`SEN-4`, `SEN-5`) -- the current 1 Hz / 1 Hz / 100 Hz split is a
  placeholder, not a measured requirement.
- The selected onboard storage method and its available memory/write budget
  (`STO-1`, `STO-4`).
- How recording starts and stops in flight: ground command, power-on,
  deployment switch, or timer (`CTL-3`).

This is a coordination task, not a coding task, but it should happen first
because it determines whether the rest of fall is spent on driver code,
porting, or both.

## 2. Replace simulated sensors with real drivers behind the existing interface

Once SEN-7 and PLT-1 are answered, implement real sensor classes against
`include/payload/ISensor.hpp` (the same interface `PressureSensor`,
`TemperatureSensor`, and `VibrationSensor` already implement in
`include/payload/SimulatedSensors.hpp`). Because the sampling loop and
`DataLogger` only depend on that interface, this should not require
changing `src/main.cpp`'s scheduling or logging logic -- only swapping which
concrete sensor classes get constructed. Validate against a bench setup
before flight integration, and confirm the placeholder bounds/units in
`SimulatedSensors.hpp` and `docs/DATA_FORMAT.md` against real datasheets.

## 3. Port storage and recording control to the selected onboard target

Two things are still ground/dev stand-ins and need to become real:

- `DataLogger` currently writes to a local filesystem path (`STO-2`). Once
  the onboard storage method is chosen (SD card, onboard flash, etc.),
  confirm the write throughput and available space against the sampling
  rates from step 1, and adjust the flush strategy in
  `src/DataLogger.cpp` if needed (it currently flushes every row for
  safety, which may be too slow for a high write budget on constrained
  storage).
- Recording currently starts on program launch and stops on `Ctrl+C` or a
  timer (`CTL-4`). Replace this with the real flight start/stop trigger
  from `CTL-3` once defined, keeping the same safe-shutdown path
  (`DataLogger::close()`) so a real trigger loses no data compared to the
  current Ctrl+C path.
