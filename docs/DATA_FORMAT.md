# Data Format

Satisfies requirements FMT-1 through FMT-5 in `docs/REQUIREMENTS.md`.

## Why one row per reading, not one row per timestamp

The vibration sensor is meant to sample much faster than pressure and
temperature (100 Hz vs 1 Hz here, both placeholders pending electrical). A
single fixed-width row of `time, pressure, temperature, vibration` would
force a choice between padding 99 out of 100 rows with stale
pressure/temperature values, or bucketing/averaging vibration data before
it is even saved. Instead, each saved row is one measurement from one
sensor, identified by a `sensor` column. This keeps the raw vibration
samples intact and makes it trivial to add a fourth or fifth sensor later
without changing the file format.

## CSV columns

| Column | Type | Meaning |
|---|---|---|
| `time_ms` | integer | Milliseconds elapsed since the start of the recording session (not wall clock; see FMT-2). |
| `sensor` | string | One of `PRESSURE`, `TEMPERATURE`, `VIBRATION`. |
| `value` | float | The reading, in the sensor's native unit (kPa for pressure, degrees C for temperature, g for vibration -- placeholders until real sensors are chosen). |
| `status` | string | `OK`, `TIMEOUT`, `OUT_OF_RANGE`, `COMM_ERROR`, `SENSOR_OFFLINE`, or `STORAGE_ERROR`. See `docs/REQUIREMENTS.md` ERR-1..ERR-3. |

## Example

```csv
time_ms,sensor,value,status
0,PRESSURE,101.3,OK
0,TEMPERATURE,22.0,OK
0,VIBRATION,0.01,OK
10,VIBRATION,-0.03,OK
20,VIBRATION,0.05,OK
...
1000,PRESSURE,101.28,OK
1000,TEMPERATURE,22.02,OK
```

## Reconstructing a per-sensor time series

To plot pressure, temperature, or vibration over time, filter rows by the
`sensor` column and sort/keep them in `time_ms` order (they are already
written in order). `scripts/plot_data.py` does exactly this.

## Known open items

* Whether the flight computer's real clock source should replace the
  "elapsed ms since start" convention (FMT-2) is TBD with Payload
  Electrical -- elapsed time avoids needing a battery-backed RTC for the
  first program.
* Units and physically plausible bounds (used for `OUT_OF_RANGE` detection)
  are placeholders and must be revisited once real sensor datasheets are
  available (SEN-7).
