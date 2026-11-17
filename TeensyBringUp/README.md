# Teensy 4.1 sensor bring-up

This PlatformIO firmware polls a BMP390 at 25 Hz on I2C0 (Wire, pins 18 and 19)
 and an ADXL375 at 800 Hz on I2C1 (Wire1, pins 17 and 16) of the Teensy. Each
sensor has it's own bus. It checks both device IDs, reports sensor/I2C errors 
without stopping the loop, captures a ground-pressure reference on the first good 
BMP390 sample, and outputs relative altitude and acceleration.

## Build and run

Open this directory in PlatformIO and build/upload the `teensy41` environment.
Open the serial monitor at 115200 baud.

Serial output is CSV-like:

* `A,sequence,x_g,y_g,z_g` — every 8th ADXL375 sample; sensitivity is 0.049 g/LSB.
* `B,micros,pressure_hPa,temperature_C,altitude_m` — BMP390 data. Altitude is
  relative to the average of the first 50 valid pressure samples.
  `GROUND,pressure_pa=...` - Printed once the groun refernce is captured
  `BMP_ERROR,reg=0x..` — printed when a BMP390 read fails. `0xFF` means the
  error register could not be read.
* `I2C,<bus>_<sensor>_<transfer>,count=...,avg_us=...,max_us=...` — measuredsensor read duration over the
  preceding reporting interval. These timings include software/API overhead;
  use a scope or logic analyzer for wire-only transaction duration.
* `RATE,...` — per-second `accel_hz`/`baro_hz`, cumulative `missed_accel` and
  `missed_baro`, and `errors=0x..`, the error flags set since the last report:
  bit 0 BMP390 unavailable, bit 1 ADXL375 unavailable, bit 2 BMP390 error
  register, bit 3 accelerometer overrun/possible FIFO loss, bit 4 barometer
  schedule overrun.

Send `f` to enable ADXL375 stream FIFO mode (drain every 5 ms), `b` for direct
read mode (poll every 300 us). Compare measured rates, max service times, and 
FIFO near-full events between modes. The FIFO test measures buffering/jitter 
tolerance; the ADXL375 still requires one six-byte burst read for each sample, so 
FIFO does not intrinsically reduce the sensor data bytes or per-sample I2C 
transactions.

## Bus budget

**I2C0 (BMP390)**

| Transfer | Rate | Bytes | Est. wire time | Est. bus use | Measured avg / max |
|---|---:|---:|---:|---:|---|
| Pressure/temperature read | 25/s | 9 | 203 us | 0.51% | Run firmware |
| Error-register read | 25/s | 4 | 90 us | 0.23% | Run firmware |
| **Total** | — | — | — | **about 0.74%** | |

**I2C1 (ADXL375)**

| Transfer | Rate | Bytes | Est. wire time | Est. bus use | Measured avg / max |
|---|---:|---:|---:|---:|---|
| Data burst (pointer + six bytes) | 800/s | 9 | 203 us | 16.2% | Run firmware |
| Status read, FIFO mode (2 reads per poll) | 400/s | 4 | 90 us | 3.6% | Run firmware |
| Status read, direct mode (1 read per poll) | ~3333/s | 4 | 90 us | ~30% | Run firmware |
| **FIFO-mode total** | — | — | — | **about 19.8%** | |
| **Direct-mode total** | — | — | — | **about 46%** | |


## Bench validation

The firmware provides telemetry and error detection but cannot establish
hardware pass results by itself. Record the board, supply, and observed values
when performing these tests:

1. **Still test:** run for two minutes with the board stationary. Verify
   `missed_accel=0`, `missed_baro=0`, no error flags, roughly 800 acceleration
   records/s and 25 barometer records/s. Note pressure/altitude drift.
2. **Height test:** keep the board still, then move it to a known higher
   location (elevator or stairwell). Confirm reported relative altitude
   increases and record the known and observed height change. Avoid rapid
   pressure changes from handling or airflow.
3. **Shake test:** shake the board safely by hand and confirm the acceleration
   output responds on all axes without persistent overrun flags.
4. **FIFO comparison:** run a still/shake interval in direct mode, then send
   `f` and repeat. Compare throughput, maximum service time, schedule misses,
   and FIFO near-full events. FIFO mode should be judged by measured results;
   its status polling adds bus traffic and the per-sample data reads remain.
5. **Disconnect test:** unplug each sensor separately while powered only if
   safe for the specific breakout. Confirm the corresponding error flag appears,
   firmware continues running, and reconnecting clears the unavailable flag
   after successful retry.
6. **Ten-minute run:** leave both sensors connected and verify the cumulative
   missed counts remain zero. Save serial output as the run record.

| Test | Result / measurements | Pass |
|---|---|---|
| Two-minute stationary run | Not run (requires hardware) | — |
| Known height change | Not run (requires hardware) | — |
| Shake test | Not run (requires hardware) | — |
| Ten-minute rate run | Not run (requires hardware) | — |
| Individual sensor disconnect/reconnect | Not run (requires hardware) | — |
