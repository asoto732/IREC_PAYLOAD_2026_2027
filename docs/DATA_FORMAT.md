# Data Format

Satisfies requirements FMT-1 through FMT-7 in `docs/REQUIREMENTS.md`.

## Why one row per reading, not one row per timestamp

The selected sensors sample at very different rates: the ADXL375 at 800 Hz,
the BMP390 at 100 Hz, and the SHT40 at 1 Hz. A single fixed width row of
`time, pressure, temperature, vibration, ...` would force a choice between
padding 799 out of 800 rows with stale values, or bucketing/averaging
vibration data before it is even saved. Instead, each saved row is one
measurement from one sensor, identified by a `sensor` column. This keeps the
raw vibration samples intact, and it is why adding humidity, and later
removing the GPS streams, changed which rows appear rather than the file
format itself.

## Why microseconds

Timestamps are elapsed **microseconds** since the start of the session. The
800 Hz vibration stream has a 1.25 ms period, which millisecond timestamps
cannot represent — samples would land on 1 ms, 2 ms, 3 ms and the real
spacing would be lost. Every stream shares one clock (FMT-6), so the
pressure and vibration series can be lined up exactly in analysis.

Logs written before this change use a `time_ms` column;
`scripts/plot_data.py` still reads those.

## CSV columns

| Column | Type | Meaning |
|---|---|---|
| `time_us` | integer | Microseconds elapsed since the start of the recording session (not wall clock; see FMT-2). |
| `sensor` | string | One of `PRESSURE`, `TEMPERATURE`, `VIBRATION`, `HUMIDITY`. See the proposed addition below for reefing events. |
| `value` | float | The reading, in that stream's unit (see below). |
| `status` | string | `OK`, `TIMEOUT`, `OUT_OF_RANGE`, `COMM_ERROR`, `SENSOR_OFFLINE`, or `STORAGE_ERROR`. See `docs/REQUIREMENTS.md` ERR-1..ERR-3. |

## Streams and units

| `sensor` | Source part | Rate | Unit | Range logged as valid |
|---|---|---|---|---|
| `PRESSURE` | BMP390 | 100 Hz | kPa | 30 – 125 |
| `TEMPERATURE` | BMP390 | 100 Hz | °C | −40 – 85 |
| `VIBRATION` | ADXL375 | 800 Hz | g | −200 – 200 |
| `HUMIDITY` | SHT40 | 1 Hz | % RH | 0 – 100 |

Values are written with 7 significant digits. The stream default of 6 would
quantize a ~101.3 kPa reading to 1 Pa, coarser than the BMP390's ±3 Pa
relative accuracy; 7 digits gives 0.1 Pa granularity (FMT-7). This matters
more than it looks: pressure is the only altitude source on the payload, so
every flight state transition and every reefing trigger is derived from
this column.

GPS was removed from the payload, so `GPS_LATITUDE`, `GPS_LONGITUDE`, and
`GPS_ALTITUDE` are no longer written. Logs recorded before the removal still
contain them, and `scripts/plot_data.py` plots any stream it finds, so those
files stay readable.

## Which statuses carry a value

Not every non `OK` row has a meaningful number in `value`, and post flight
tools should not treat them the same:

| Status | `value` field |
|---|---|
| `OK` | The reading. |
| `OUT_OF_RANGE` | The offending reading, kept so the excursion is visible. |
| `TIMEOUT` | The sensor did not respond in time; no reading was produced. |
| `COMM_ERROR`, `SENSOR_OFFLINE`, `STORAGE_ERROR` | No reading was produced; the field is a placeholder. |

`scripts/plot_data.py` follows this: rows with no reading are drawn as marks
along the bottom of the panel rather than plotted as zero.

## Example

```csv
time_us,sensor,value,status
2,PRESSURE,101.2199,OK
2,TEMPERATURE,21.79863,OK
2,VIBRATION,-0.5220748,OK
2,HUMIDITY,45.05422,OK
1253,VIBRATION,0.0789703,OK
2503,VIBRATION,-0.127791,OK
...
10002,PRESSURE,101.2141,OK
10002,TEMPERATURE,21.80912,OK
```

## Reconstructing a per sensor time series

To plot any stream over time, filter rows by the `sensor` column and
sort/keep them in `time_us` order (they are already written in order).
`scripts/plot_data.py` does exactly this, and also reports each stream's
measured rate so a degraded sample rate shows up instead of hiding.

## Size

At the rates above the CSV runs about 31 KB/s (≈956 KB for the 30 second
sample run in `data/`), down from ≈37 KB/s before the GPS streams were
removed. A recording that covers pad arming through recovery
is a few tens of MB — the storage budget in `docs/HARDWARE.md` — against an
8–32 GB card, so the readability of plain text is worth the size.

## Proposed addition: flight state and reef stage events (TBD)

For the parachute reefing experiment (see `docs/REQUIREMENTS.md` section 10
and `docs/REEFING_ALGORITHM.md`), the recommendation is to log state
transitions and reefing stage triggers as ordinary rows on the same timeline,
rather than a separate file, so a post flight plot can show exactly where
each event landed relative to pressure, altitude, and vibration. This is a
proposal only; it is not implemented yet (REEF-6).

| `sensor` | Meaning | `value` |
|---|---|---|
| `FLIGHT_STATE` | A flight state transition from `docs/FLIGHT_STATE_ALGORITHM.md` (pad, boost, coast, apogee, drogue descent, main descent, landed). | Integer state code. |
| `REEF_STAGE` | A reefing stage trigger (motor commanded to unwind the spool) from `docs/REEFING_ALGORITHM.md`. | Integer stage number (0 = reefed deploy, 1 = first disreef, 2 = full disreef). |

Both would use the existing `status` column to distinguish a normal altitude
triggered event (`OK`) from a backup timer triggered event (a new status,
e.g. `TIMER_FALLBACK`), so it is visible after the flight whether a stage
fired on its primary or backup trigger.

## Known open items

* Whether all three ADXL375 axes get their own stream (SEN-12). That would
  add `VIBRATION_X/Y/Z` and triple the vibration data rate; the format
  itself needs no change for it.
* Whether the flight computer should also record a wall clock time
  alongside elapsed time, for correlating with ground side logs. GPS time
  was the free way to get this; without a receiver it needs an RTC or a
  ground-set epoch written at arming.
* The `FLIGHT_STATE` / `REEF_STAGE` schema addition above, once the reefing
  experiment design is approved at PDR.
