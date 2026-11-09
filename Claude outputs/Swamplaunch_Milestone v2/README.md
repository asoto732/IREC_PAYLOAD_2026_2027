# Swamplaunch: laptop logger (build steps 6.2 and 6.3)

This program exercises the flight logger on your laptop. It generates fake
accelerometer and barometer readings, runs them through the logger's RAM queues,
and saves a CSV file that the plotting team can use before the flight hardware
is ready. It implements section 6, steps 2 and 3 of P4A_SD_LOGGER.md.

The logger is the reusable component. The fake flight is a way to test and
demonstrate it. Flight states in this demo change at scheduled times; they are
not detected from sensor readings. The program currently writes to your laptop's
disk. Real sensors and Teensy microSD storage come in the later build steps.

## Run with PlatformIO on this Windows machine

The completed project is in **Swamplaunch_Milestone v2**. Open that folder in
VS Code, then open a **PowerShell terminal**. The commands below use the portable
LLVM/MinGW compiler already in its `.tools` folder. It includes a `g++` command
backed by Clang, so you do not need a separate GCC installation.

First, select the project and run this in the terminal:

```
cd "$env:USERPROFILE\Downloads\Swamplaunch_Milestone v2"

$env:PATH = (Resolve-Path '.tools\llvm-mingw-20241217-ucrt-x86_64\bin').Path + ';' + $env:PATH

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e native -t exec
```

The test command builds and runs 15 cases. A successful result reports
`15 test cases: 15 succeeded`. The second command builds and runs `main.cpp`,
then prints a message such as:

```text
Wrote logs\LOG0004.CSV (10 flight seconds + 1 pad second; 800 Hz ADXL375, 25 Hz BMP390; 0 recording rows dropped)
```

The number depends on the files already present. Each run creates the next free
`LOGxxxx.CSV`; it preserves existing logs. To check the newest generated file:

```powershell
$latestCsv = Get-ChildItem -LiteralPath '.\logs' -Filter 'LOG*.CSV' -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
& $pythonExe .\verify.py $latestCsv.FullName
```

`verify.py` checks the CSV's columns, exact sample counts and timestamps, event
sequence, state coverage, units example, and blank cells. It is a separate check
of the generated output, whereas `pio test` tests the logger's behavior directly.

To generate a five-second flight instead of ten:

```powershell
& $pioExe run -e native -t exec -a logs -a 5
$latestCsv = Get-ChildItem -LiteralPath '.\logs' -Filter 'LOG*.CSV' -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
& $pythonExe .\verify.py $latestCsv.FullName 5
```

The first argument is the output directory; the second is a flight duration from
1 to 3600 whole seconds. Every run also includes one simulated pad second.
Pass the same flight duration to `verify.py`; its default is ten seconds.

The PATH change and PowerShell variables apply to this terminal session. Repeat
the setup block when you open a new terminal. Use these commands in that terminal
to build and run. If you move the project, adjust the `Set-Location` line.
The portable compiler folder must be present for this setup to work.

If `pio`, `python`, and a C++17 compiler are already on PATH, the shorter commands
are `pio test -e native`, `pio run -e native -t exec`, and
`python verify.py <generated-file>`. PlatformIO's
[native platform uses a compiler on PATH](https://docs.platformio.org/en/latest/platforms/native.html).
The supplied configuration pins native platform 1.2.1 and keeps downloads and
build files inside `.pio-core` and `.pio` in the project.

## What happens during a run

1. `main.cpp` creates a `FileSink` for the output directory and a `Logger` that
   uses that sink. `begin()` creates a new numbered file and enters IDLE.
2. `SyntheticFlight.hpp` advances a virtual clock in steps of 1250 microseconds.
   Each step creates an ADXL375 acceleration row. Every 40000 microseconds it
   also creates a BMP390 altitude/pressure/temperature row.
3. During PAD IDLE, `push()` keeps recent rows in two rolling prelaunch buffers.
   No measurement rows are written yet. Old pad history is deliberately replaced
   as those buffers fill.
4. At the scheduled BOOST transition, the simulator calls `startRecording()`.
   The logger retains the recent pad history, starts the versioned CSV, and
   accepts new samples and events into the recording queues.
5. The simulator keeps calling `service()`. That function takes queued rows,
   formats them as CSV, and writes batches to the file through `FileSink`.
6. At the scheduled LANDED transition, the simulator pushes `STATE:LANDED` and
   calls `close()`. Further service passes drain remaining rows, append
   `LOG_STOP`, flush, and close the file.

The default timeline is:

| Simulated time | Flight state | What the demo does |
| --- | --- | --- |
| 0 to 1 s | PAD IDLE (0) | Keeps recent measurements in RAM |
| 1 to 3 s | BOOST (1) | Starts recording; emits launch state/flag events |
| 3 to 6 s | COAST (2) | Continues recording |
| 6 to 7 s | DESCENT (3) | Emits the apogee state/flag/altitude events |
| 7 to 8 s | DEPLOY (4) | Records the scheduled state change |
| 8 to 11 s | MAIN DESCENT (5) | Continues recording |
| 11 s | LANDED (6) | Records landing and completes a clean stop |

These are **virtual seconds**: the program runs as fast as computation and file
output allow. An 800 Hz stream here means timestamps are 1250 microseconds apart;
it does not demonstrate 800 real sensor reads per wall-clock second.

The fake altitude rises to 120 m and returns toward the ground. Pressure and
temperature vary with altitude, and acceleration also varies. These are useful
plotting examples, not a validated flight dynamics model. The default demo
generates BMP390, ADXL375, and EVENT rows. ACT and CTRL formatting is implemented
and tested, ready for the actuator and controller owners to supply those rows.
Status is `0x0` in the demo; the shared status bit layout remains to be agreed.

## Why there are queues

`push(row)` copies a validated measurement into fixed RAM storage. It does not
format text, allocate memory, sleep, or call storage. `service(now_ms)` does the
formatting and storage work separately.

Recording has three queues: events, slow samples (BMP390/ACT/CTRL), and fast
samples (ADXL375). Events drain first, then slow samples, then fast samples.
Consequently, the file can contain rows out of timestamp order across streams;
the plot script should sort by `t_us`.

When a sample queue fills, it discards its oldest sample and increments
`droppedRows()`. A full event queue rejects the incoming event and counts that
loss, preserving events already queued. A `ROWS_DROPPED:<count>` event is added
when space is available. Trimming old PAD history is intentional and is not
counted as a recording drop.

Each service pass attempts at most one data write. Normal writes use whole
512-byte blocks from a 4096-byte staging buffer. Scheduled flushes also write
the incomplete tail; the flush itself runs on a following pass. The provisional
flush interval is 1000 ms of the supplied clock. Writes and flushes get up to
three additional attempts on later passes before the logger enters FAILED.
Short-write retries send only the unaccepted bytes, avoiding duplicate CSV text.

These queues separate acquiring a row from writing it. A synchronous storage
call can still block `service()`, so protecting real sensor timing requires
hardware scheduling work and measurements as well as buffers.
`Logger` and `RingBuffer` are single-threaded; sharing them with an ISR or another
thread requires an appropriate synchronization design.

## Output and useful files

With the default ten-second flight, a file contains:

| Row kind | Count | Explanation |
| --- | --- | --- |
| ADXL375 | 8512 | 8000 flight samples plus 512 retained pad samples |
| BMP390 | 275 | 250 flight samples plus all 25 pad samples |
| EVENT | 13 | Version, ground reference, states, flags, apogee, and stop |

The 512-row pad acceleration buffer retains the last 0.64 s of its one-second
pad wait. Thus ADXL375 timestamps start at 360000 us and remain 1250 us apart.
BMP390 timestamps start at zero and remain 40000 us apart. `LOG_START:V1`
timestamps the earliest retained history, and `LOG_STOP` is at 11000000 us.
There are 8800 data rows plus the header. Zero recording drops is the expected
result; it does not mean every old pad sample was retained.

All five row kinds use this fixed 24-column format, leaving irrelevant cells blank:

```text
t_us,sensor,flight_state,reef_state,status,alt_m,press_pa,temp_c,ax_g,ay_g,az_g,act_state,grants,motor_pos,motor_a,rail12_v,rail24_v,tgt_rate,meas_rate,drag,err,cmd_width,meas_width,event
```

BMP390 values are altitude in m, pressure in Pa, and temperature in C. ADXL375
values are x/y/z in g. ACT includes optional encoder counts, left blank unless
`has_motor_position` is true. CTRL carries the six fields in section 4.1 of the
logger specification; unsettled units remain for the interface owners to agree.
Event text is 1 to 127 bytes without commas, quotes, or line breaks.

| File | Purpose |
| --- | --- |
| `main.cpp` | Selects disk output, reads arguments, starts the synthetic run |
| `SyntheticFlight.hpp` | Produces fake measurements and scheduled flight events |
| `DataFormat.hpp`, `CsvFormat.hpp` | Define row fields and format version 1 CSV |
| `Logger.hpp`, `RingBuffer.hpp` | Buffer rows, manage recording, retries, flushing, and closing |
| `LogSink.hpp` | Defines `ILogSink`, the logger's storage interface |
| `FileSink.hpp` | Creates numbered laptop files without overwriting existing ones |
| `MemorySink.hpp` | Holds test output in memory and injects storage faults/delays |
| `test_logger.cpp`, `tests/` | Run the same named tests standalone or through PlatformIO |
| `verify.py` | Checks a generated synthetic CSV |
| `demo.csv` | Supplied sample file for the plotting team |

## Which requirements this covers

The following IDs come from P4A_SD_LOGGER.md. "Laptop coverage" means implemented
and exercised here; it does not establish the hardware portions of a requirement.

| Requirement IDs | Laptop coverage / evidence | What remains |
| --- | --- | --- |
| FMT-1, FMT-4, FMT-5, LOG.9 | Timestamp and sensor columns; fixed CSV header; `LOG_START:V1`; LT1, LT2 | Keep version and column changes coordinated with consumers |
| FMT-2 | Monotonic virtual measurement timestamps; LT2 | Supply extended, monotonic `micros()` time on hardware, including wrap handling |
| LOG.1, CTRL.8 | All five row layouts and their fields; LT1 | Real P1/P2/P3 data; one CTRL row each controller cycle; final field conventions |
| LOG.2 | Event storage and formatting; synthetic state, flag, and apogee events; actuator-command event tested; LT4, LT14 | Each real module must emit every required state/flag/command event |
| LOG.5, CTL-3 | PAD prebuffers; explicit BOOST start and LANDED stop in simulator; LT3, LT14 | Connect calls to the real flight state machine and arming behavior |
| LOG.3, LOG.6 | RAM queues, event priority, overflow counts, retries, scheduled flushing; LT4, LT5, LT12 and extra cases | Prove sensor/controller timing with actual SD writes and the chosen scheduler |
| ERR-2 | Write/flush failures detected; bounded retries; `storageFailed()`; LT5 and extra cases | Map storage faults into the shared flight fault/status reporting |
| ERR-5 | Final drain, flush, and safe repeated close; LT6 | Trigger close on real sensor loss; test card removal and power-loss behavior |
| STO-3 | Exclusive numbered disk files; existing files preserved; LT7 | Implement the same behavior in the Teensy SD sink |
| LOG.7 | Status word in every row; supplied fault events and all 32 bits preserved; LT13 | Agree bit assignments/fault codes and connect actual fault sources |
| LOG.8 | Simulated reset opens a new file, names the previous file, adds saved elapsed time; LT8 | Nonvolatile state/time saving, boot recovery, and reboot-gap validation |
| LOG.11, PLT-3 | Logger accesses storage only through `ILogSink`; exercised with memory and disk sinks | Implement and compile the Teensy sink against this interface |
| TST.1, TST.2 | Laptop flight simulation and named tests for this milestone; table below | Complete traceability and tests for the remaining hardware/integration work |
| LOG.4 | Deferred | Teensy 4.1 built-in SDIO microSD support |
| STO-4, PLT-4 | Virtual stream exercises the data path; `push()` performs RAM work only | Measure throughput and worst-case execution time; prove real-time acquisition |
| LOG.10 | Settings are exposed as provisional configuration | Choose sizes and flush interval from section 7 bench measurements |

The logger carries caller-supplied states, faults, and status bits. It does not
decide when the rocket launched or automatically discover every flag change.
A storage failure is exposed through `storageFailed()`; an unusable sink cannot
be relied on to save its own final failure event.

## Tests and their limits

| Test | Coverage |
| --- | --- |
| LT1 | One header; five row layouts; blank cells; optional encoder; integer precision |
| LT2 | Exact synthetic 800/25 Hz counts and measurement timestamps |
| LT3 | Retained PAD history, buffer wrap, copied event text, no IDLE row writes |
| LT4 | Oldest sample eviction, correct drop count, event/slow/fast priority |
| LT5 | Missing storage, failed writes, bounded retries, pushes after failure |
| LT6 | Clean final drain, `LOG_STOP` last, repeated close, idle/failure close |
| LT7 | Numbered disk files and preservation of existing files |
| LT8 | Simulated reset, previous filename, restored 64-bit elapsed time |
| LT12 | Artificial sleeping sink and a 100 ms service gap absorbed without drops |
| LT13 | Caller-supplied fault events and all 32 status bits |
| LT14 | Scheduled BOOST-to-LANDED lifecycle |

Four additional cases cover partial-write integrity, full event queues, flush
intervals/errors/block boundaries, and malformed input/ring behavior. Together
these make 15 test cases. The existing implementation passed them in PlatformIO's
native environment; the commands above let you rerun them on this machine.

LT12's 100 ms service gap and 2 ms injected sleep are artificial. It models rows
being supplied while storage service is delayed; it does not demonstrate actual
concurrent acquisition or use a measured card stall. LT9-LT11 still require the
bench. LT15 needs the external plot script, which is not included. LT16 applies
only if measurements lead to implementing the optional lean text format.

## Suggested next steps

1. **Check the plotting handoff now.** Give the plot owner `demo.csv` or a
   generated log and run their script against it (LT15). Agree column meanings,
   status bits, controller units, actuator codes, and event names with P1/P2/P3.
2. **Build the Teensy SD sink (step 4).** Implement `ILogSink` using the built-in
   SDIO slot, unique filenames, file preallocation, writes, flush, and safe close.
   Start with serial/blink checks and the existing synthetic stream. Retain
   accepted-byte counts and flush-error reporting from this interface.
3. **Connect the real producers (step 5).** Replace the generator with P1 sensor
   rows, P2 flight/controller rows and events, and P3 actuator rows/events. Have
   the state machine call start at BOOST and request close at LANDED or sensor
   loss. Resolve acquisition/storage scheduling and timestamp wrap handling.
4. **Measure on the flight card class (step 6).** Fill section 7's table: worst
   write/flush times, sustained throughput, `push()`/`service()` duration, queue
   high-water marks, and drops. Run the ten-minute 800/25 Hz bench stream and
   card-removal tests (LT9-LT11).
5. **Choose measured settings (step 7).** Set queue capacities, PAD history, and
   flush interval from those results. Check RAM placement/row size on Teensy,
   and decide CSV versus the optional lean text format from measured throughput.
6. **Finish persistent reset recovery (step 8).** Integrate the FLT.5 state record,
   save elapsed time and previous filename, recover at boot, and test a real
   reset in flight. Validate that adjacent files can be joined correctly.

The current capacities are placeholders: pre slow/event 256 rows, pre fast 512,
events 64, slow samples 256, and fast samples 1024, with a 1000 ms flush interval.
Tests can select smaller nonzero logical capacities up to those fixed maxima.
Owned event text and desktop doubles make stored rows larger than the original
sketch's estimate; do not assume its 40 KB figure. The demo allocates Logger on
the heap to avoid Windows stack limits. Teensy storage placement and memory
use need review before deployment.

For recovery tests, call `recoverAfterReset(previousFile, savedElapsedUs, flight,
reef, status)` after `begin()`, instead of `startRecording()`, before any new rows.
It uses the file that `begin()` just allocated, emits the recovery prologue, and
adds the saved offset to subsequent boot-relative timestamps exactly once. The
state owner must supply the saved information; this project does not persist it.

PROJECT_SPEC.md describes the separate ESP32 BMP390 learning exercise. Its GPIO
13/14 wiring, LittleFS storage, and four-column output are specific to that
exercise. Use its sensor information when planning integration, and confirm
the actual flight hardware wiring and interfaces for the Teensy implementation.

## Other native build options

With an existing C++17 compiler, or the portable compiler on PATH:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic test_logger.cpp -o test_logger.exe
.\test_logger.exe
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic main.cpp -o demo.exe
.\demo.exe logs 10
```

On Linux/macOS, add `-pthread`, omit `.exe`, and execute with `./`.
CMake users can run `cmake -S . -B build`, `cmake --build build`, and
`ctest --test-dir build --output-on-failure`; add `-C Debug` for a multi-configuration
build. The CMake recipe is an alternative; validation so far used Clang and PlatformIO.
