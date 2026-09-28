# Recommended First Steps for Fall Semester

The hardware is selected (`docs/HARDWARE.md`), so the software's open
questions are no longer "which parts" — they are "does this work on the
real parts." These three steps match the electronics research's own fall
plan, ordered by what blocks the most other work.

## 1. Breadboard the BMP390 + ADXL375 on the Teensy 4.1 and confirm logging at full speed

This is the step that turns everything else from an assumption into a
measurement. Build the two flight-critical sensors on a breadboard with the
Teensy 4.1 and a microSD card, port the sampling loop in `src/main.cpp`, and
implement real `ISensor` drivers for the two parts. The loop and
`DataLogger` only depend on that interface (PLT-3), so this should be new
driver classes plus a constructor change in `main()`, not a rewrite.

What to verify while it is on the bench:

- The vibration stream actually holds **800 Hz** on the real SPI bus, not
  just in simulation. The plotting script prints each stream's measured rate
  (`scripts/plot_data.py`), so this is a one-command check against a real
  log.
- The microSD keeps up with `config::kFlushIntervalMs` (250 ms). If writes
  stall, that constant and the buffering strategy in `src/DataLogger.cpp`
  are the knobs — `STO-4` is sized from a development-machine measurement,
  not from the real card.
- Both sensors on separate SPI peripherals behave as the pin plan in
  `docs/HARDWARE.md` assumes. Confirm that plan against the real schematic
  while the board is in front of you (PLT-5).

Add the SHT40 once the two critical sensors are solid; it is a slow stream
and should not be what makes the first bring-up hard.

## 2. Measure real power draw, size the battery and regulator, and test recovery from a brief dip

Power sag at ignition is on the risk list, and nothing in software can
substitute for a measurement. With the breadboard from step 1 running,
measure the actual draw of the Teensy plus all three sensors, size the
battery and regulator with margin, then deliberately brown out the supply
mid-recording. Removing the GPS took ≈31 mA of tracking current off this
budget, so the sensor side is now dominated by the Teensy itself.

The software requirement to check is `ERR-6`: a dip should cost at most one
flush window, and the file must still open and parse afterwards. Run the
partial file through `scripts/plot_data.py` — it skips a truncated final
row rather than failing, so a clean plot from a deliberately interrupted run
is the pass condition.

## 3. Shake-test the prototype and settle the two open decisions it answers

Shake-test the prototype (or fly it non-critically) to confirm mounting and
that 800 Hz actually captures what the structures team needs. Two open items
resolve off that data:

- **`SEN-12`: one vibration channel or three.** The ADXL375 measures X, Y
  and Z; the software logs one channel today. Real shake data shows whether
  the other two axes carry information worth tripling the vibration data
  rate for.
- **`CTL-3`: how recording starts and stops in flight.** This is the last
  blocking `TBD` and it is a systems decision — ground command, power-on,
  deployment switch, or timer. Whatever is chosen, it replaces the
  launch-on-start/Ctrl+C stand-in (`CTL-4`) and must route through the same
  `DataLogger::close()` path so a real trigger loses no more data than the
  current one.

One test to add now that the single barometer is a settled decision
(`FSD-5`): run a logging session with the BMP390 deliberately failed —
disconnected, or forced to return faults — and confirm the software behaves
as the failsafe assumes. The disreef stages must fall through to their
backup timers (REEF-4) and the landing safety net must still fire anything
left. Those timers are load bearing now, not a rarely exercised path, so
they deserve a deliberate test rather than an assumption.
