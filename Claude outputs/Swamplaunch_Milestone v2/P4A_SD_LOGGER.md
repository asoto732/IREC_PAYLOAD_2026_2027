# P4A: SD Logger (mini project)

Legend: `BASELINE` = decided, `TBD` = open, `ASSUMED` = placeholder so development can proceed.

## 1. Purpose

The logger is the firmware module that turns a flight or a bench test into a readable record. It writes timestamped sensor rows, state and flag changes, actuator events, controller fields, and fault codes to the Teensy 4.1 microSD card, without ever delaying a sensor read or the controller. It is the evidence source for most other requirements (the controller tuning, the fault tests, the traceability table), and the plot script and test harness in P4 read what it writes.

## 2. Scope

In scope:
* The `Logger` class, its queues, and its row formatting.
* The `ILogSink` interface with a Teensy SD sink and a laptop memory sink.
* The log format (columns, row kinds, events, status word, file naming).
* Behavior on storage faults and on a reset in flight.
* Native tests and the bench measurements.

Out of scope (stay in P4, but this file defines what they consume): the plot script, the OpenRocket profile generator, and the fault injection harness.

## 3. Requirements

| ID | Requirement | Source | Status |
|----|-------------|--------|--------|
| LOG.1 | The log shall carry timestamp, altitude, pressure, temperature, acceleration, flight state code, reefing state code, flag states, actuator state, grants, motor position (if an encoder is fitted), faults, rails, and the controller fields in CTRL.8. | Reefing doc | BASELINE |
| LOG.2 | Event rows shall record every state change, every flag change, the apogee event, and every actuator command. | Reefing doc | BASELINE |
| LOG.3 | The log shall be flushed on a fixed interval and closed safely on stop. 800 Hz acceleration rows shall be buffered so an SD write cannot delay a sensor read. | Reefing doc | BASELINE |
| LOG.4 | Storage shall be the Teensy 4.1 built in microSD slot over SDIO. | CDR | BASELINE, confirm a card is fitted |
| LOG.5 | Recording starts at BOOST and stops at LANDED. Samples from PAD IDLE shall be kept in a short ring buffer and written when recording starts. | CDR, SW | BASELINE |
| LOG.6 | Storage faults shall not affect flight logic or sensor timing. Rows go to a RAM buffer first. A write error is retried without blocking sensor reads. If the buffer fills, the oldest high rate acceleration rows are dropped before any event row, and the number of dropped rows is counted and logged. Event rows have priority over sample rows. | Fault handling | BASELINE, buffer size TBD |
| LOG.7 | Faults shall be logged as a fault code and a status word, with the status word as a column in every row. | Fault handling | BASELINE, layout TBD |
| FMT-1 | Every measurement shall be timestamped. | REQUIREMENTS.md | BASELINE |
| FMT-2 | Timestamps shall be monotonic elapsed time from `micros()`. | REQUIREMENTS.md | ASSUMED (see LOG.8 for resets) |
| FMT-4 | Data shall be CSV with a header row. | REQUIREMENTS.md | BASELINE, see section 4.6 |
| FMT-5 | The CSV shall carry a `sensor` column so streams with different rates share one file. | REQUIREMENTS.md | BASELINE |
| STO-3 | A new uniquely named file per recording session. | REQUIREMENTS.md | BASELINE |
| STO-4 | Write throughput shall support 800 Hz acceleration rows without delaying sensor reads. | REQUIREMENTS.md | BASELINE, verify on bench |
| ERR-2 | Storage write failures shall be detected and reported. | REQUIREMENTS.md | BASELINE |
| ERR-5 | The data file shall be flushed and closed safely on stop or sensor loss. | REQUIREMENTS.md | BASELINE |
| CTL-3 | In flight, recording starts when BOOST is detected and stops at LANDED. | REQUIREMENTS.md | BASELINE, arming input TBD |
| PLT-3, PLT-4 | Storage access sits behind a small interface. Calls shall never block and shall take bounded time. | REQUIREMENTS.md | BASELINE |
| CTRL.8 | Each controller cycle logs target and measured descent rate, drag, error, and commanded and measured width. | Reefing doc | BASELINE |
| TST.1, TST.2 | The logger runs on a laptop against simulated flights, and every requirement maps to a named test. | Reefing doc | BASELINE |
| LOG.8 | After a reset in flight the logger shall open a new numbered file. Its first rows shall be an event that names the previous file, and its timestamps shall continue from the time saved with the restored state (section 5.5), so the files can be joined. | SW | ASSUMED |
| LOG.9 | The header row and the column list shall be fixed and identified by a format version written at the start of every file. Any change to the columns shall change the version. | SW | ASSUMED |
| LOG.10 | Queue sizes, the flush interval, and the pre buffer lengths shall come from the bench measurements in section 7, not from guesses. | SW | ASSUMED |
| LOG.11 | The logger code shall depend only on the `ILogSink` interface, so the same code builds for the Teensy and for the laptop tests. | SW | ASSUMED |

## 4. Log format

### 4.1 Columns (format version 1)

All rows have all 24 columns. A column that does not apply to a row kind is left blank.

| Column | Filled by | Unit and meaning |
|--------|-----------|------------------|
| `t_us` | all | Microseconds of flight time (section 5.5) |
| `sensor` | all | `BMP390`, `ADXL375`, `ACT`, `CTRL`, or `EVENT` |
| `flight_state` | all | 0 PAD IDLE, 1 BOOST, 2 COAST, 3 DESCENT, 4 DEPLOY, 5 MAIN DESCENT, 6 LANDED |
| `reef_state` | all | 0 MAIN DEPLOY, 1 PARTIAL DISREEF, 2 FULL OPEN, 3 LANDED. 0 before MAIN DESCENT. |
| `status` | all | Status word in hexadecimal (section 4.4) |
| `alt_m` | BMP390 | Meters above the ground reference |
| `press_pa` | BMP390 | Pascals |
| `temp_c` | BMP390 | Degrees C |
| `ax_g`, `ay_g`, `az_g` | ADXL375 | g |
| `act_state` | ACT | Integer code for which actuator is running and in which direction (table TBD with P3) |
| `grants` | ACT | 3 bits: motor, magnet, solenoid grant levels |
| `motor_pos` | ACT | Encoder counts, blank if no encoder |
| `motor_a` | ACT | Motor current from the DRV8243S, in A |
| `rail12_v`, `rail24_v` | ACT | Volts |
| `tgt_rate`, `meas_rate` | CTRL | Target and measured descent rate in m/s, positive while descending (to confirm with the controller owner) |
| `drag` | CTRL | Calculated drag, units TBD (item 26) |
| `err` | CTRL | Controller error in the units of the controlled variable |
| `cmd_width`, `meas_width` | CTRL | Commanded and measured width, units TBD (item 28) |
| `event` | EVENT | Event text (section 4.3) |

### 4.2 Row kinds

| Kind | Rate | Pushed by |
|------|------|-----------|
| `BMP390` | 25 Hz | P1 barometer class |
| `ADXL375` | 800 Hz | P1 accelerometer class |
| `ACT` | At a fixed slow rate and around every move, rate TBD (ACT.12) | P3 actuator manager |
| `CTRL` | `CONTROL_RATE_HZ`, only while the controller runs | P2 controller |
| `EVENT` | When it happens | Anyone |

### 4.3 Events

An event is one text field with colons and no commas.

| Event | Meaning |
|-------|---------|
| `LOG_START:V1` | First row of every file. Format version. |
| `META:GROUND_REF_PA:<value>` | Ground reference pressure, written at the start of recording |
| `STATE:<name>` | A flight or reefing state change, for example `STATE:BOOST` |
| `FLAG:<name>` | A flag set, for example `FLAG:LAUNCH` |
| `APOGEE:<altitude_m>` | The apogee event with the peak altitude |
| `ACT_CMD:<actuator>:<action>` | An actuator command, for example `ACT_CMD:MOTOR:PARTIAL` |
| `FAULT:<code>` | A fault code from `FAULT_HANDLING.md`, for example `FAULT:F07` |
| `BACKUP_TIMER:<state>` | A backup timer fired |
| `RESET_RECOVERED:<previous file>` | First event after a reset in flight |
| `ROWS_DROPPED:<count>` | Number of rows lost since the last report (LOG.6) |
| `STORAGE_FAILED` | The sink reported an unrecoverable write error |
| `LOG_STOP` | Last row of a clean file |

### 4.4 Status word

One bit per sensor, per actuator, per rail, for storage, and for the controller (LOG.7). The bit layout is open (section 12, L6) and is shared with the plot script. The logger carries the word; it does not decide its meaning.

### 4.5 File names

`LOG0001.CSV`, `LOG0002.CSV`, and so on. At startup the sink finds the next free number and never overwrites an existing file (STO-3). A reset in flight takes the next number (LOG.8).

### 4.6 CSV baseline and the lean text option

The baseline is plain text, comma separated, one header row (FMT-4). Every row carries all 24 columns, which costs about 20 extra commas per row, mostly on the 800 Hz acceleration rows.

If the bench measurement shows the card or the CPU cannot keep up (section 7), the fallback is a lean text file:
* One row per sample, starting with a kind letter (`B`, `A`, `C`, `K`, `E`), then only the fields that kind uses, in a fixed order.
* A laptop script, `tools/convert_log.py`, expands it to the full CSV above. The result must be identical to what CSV mode would have written (test LT16).
* The file is named `LOG0001.TXT`, so nobody opens an unconverted file by mistake.

The choice is made after the measurements (open item L1). Only `formatRow()` and the file name change, so the rest of the design is the same either way.

## 5. Design

### 5.1 Data flow

Sensor and state code calls `push(row)`. That is a copy into RAM and takes bounded time. The main loop calls `service()` once per pass. `service()` formats rows into a staging buffer and writes at most one run of whole 512 byte blocks per call, so no single pass can stall. All card access happens in `service()` through the sink.

### 5.2 Queues

| Queue | Holds | When full |
|-------|-------|-----------|
| Pre buffers (two ring buffers, one for BMP390 and events, one for ADXL375) | PAD IDLE history until BOOST | Overwrite the oldest |
| Events | `EVENT` rows | Count the drop, never overwrite an event |
| Slow samples | `BMP390`, `ACT`, `CTRL` rows | Drop the oldest and count |
| Fast samples | `ADXL375` rows | Drop the oldest and count (LOG.6) |

`service()` drains events first, then slow samples, then fast samples. Rows can therefore land in the file slightly out of time order. The plot script sorts by `t_us`. When any rows were dropped, an `ROWS_DROPPED` event is queued.

Sizes are placeholders until section 7 is filled in: pre buffers 256 and 512 rows, events 64, slow samples 256, fast samples 1024. A row is about 40 bytes, so the fast queue is about 40 KB. The Teensy 4.1 has a second RAM bank (`DMAMEM`) that suits these buffers.

### 5.3 Logger states

| State | Meaning | Next |
|-------|---------|------|
| IDLE | Powered, card started, filling the pre buffers | RECORDING at BOOST |
| RECORDING | Draining the queues to the card | CLOSED at LANDED, FAILED on a write error |
| FAILED | Storage unusable. Pushes still return at once and rows are counted as dropped. Flight logic is unaffected. | none (power cycle) |
| CLOSED | Final flush done, file closed. Calling close again is safe. | none |

### 5.4 Behavior on faults

| Fault | Behavior | Fault code | Req |
|-------|----------|-----------|-----|
| Card missing or fails to start | `begin()` returns false, state FAILED, report in the pre flight status. Flight logic continues. | F17 | LOG.6 |
| Write error | Retry on following passes up to a named limit, then FAILED. Never blocks. | F18 | ERR-2, LOG.6 |
| Slow write | Queues absorb it. Fast samples drop oldest only if the queue fills. | F19 if rows drop | LOG.3, LOG.6 |
| Card pulled mid run | Treated as a write error. The file is readable up to the last flush, so the flush interval bounds the loss. | F18 | ERR-5 |
| Queue full | Section 5.2 | F19 | LOG.6 |
| Close twice or after failure | No effect, no crash | none | ERR-5 |

### 5.5 Reset in flight (LOG.8)

A reset restarts `micros()` at zero, so the logger needs a time base that survives it. The firmware saves the elapsed flight time to nonvolatile memory with the state record (FLT.5), at every state change and every N seconds (open item L4). After a reset, `t_us` is the saved elapsed time plus the time since boot. The gap in the record is at most the save interval plus the reboot time, and the `RESET_RECOVERED` event says so.

On boot the logger:
1. Reads the recovered state. If the flight was already past PAD IDLE, it opens the next numbered file at once and goes straight to RECORDING, skipping the pre buffers.
2. Writes `LOG_START:V1` and `RESET_RECOVERED:<previous file>`.
3. Continues. `tools/stitch_logs.py` joins the files by `t_us`.

If a Teensy RTC with a backup cell is fitted, a wall clock time can also go in a `META` event (open item L5).

### 5.6 Interface sketches

These show the design. They are not compiled or tested. Verify the SD library calls against the version installed in PlatformIO.

```cpp
// core/log_row.h
enum class RowKind : uint8_t { Bmp390, Adxl375, Act, Ctrl, Event };

struct LogRow {
    uint32_t t_us;
    RowKind  kind;
    uint8_t  flight_state;   // 0 to 6
    uint8_t  reef_state;     // 0 to 3
    uint32_t status;         // status word (LOG.7)
    float    v[6];           // meaning depends on kind, section 4.1
    uint16_t event_id;       // Event rows only
};
```

```cpp
// core/log_sink.h
class ILogSink {
public:
    virtual bool begin() = 0;                                   // open card, pick next free file, write header
    virtual bool openNext() = 0;                                // close current file, open the next number (LOG.8)
    virtual bool write(const uint8_t* data, size_t len) = 0;    // false on a short write
    virtual void flush() = 0;
    virtual void close() = 0;                                   // safe to call twice
    virtual const char* fileName() const = 0;
    virtual ~ILogSink() = default;
};

// firmware/sd_sink.cpp : SdSink, uses the Teensy SD library on SDIO. begin() preallocates a
//   contiguous file so writes do not stall allocating clusters. flush() maps to sync().
// test/memory_sink.h   : MemorySink, keeps text in RAM. failAllWrites() and delayWrites(ms)
//   inject faults for the tests.
```

```cpp
// core/logger.h
class Logger {
public:
    explicit Logger(ILogSink& sink);

    bool begin();                    // IDLE, or FAILED if the card does not start
    void push(const LogRow& row);    // bounded time, never blocks, never fails
    void startRecording();           // at BOOST: move the pre buffers into the queues
    void recoverAfterReset(const char* previousFile);   // LOG.8
    void service(uint32_t now_ms);   // every pass of loop(): format, write one run of blocks, flush on schedule
    void close();                    // final flush, write LOG_STOP, close the sink

    bool     isRecording() const;
    bool     storageFailed() const;
    uint32_t droppedRows() const;
};
```

```cpp
// core/logger.cpp (key logic only)
void Logger::push(const LogRow& r) {
    if (state_ == State::Idle) { prePush(r); return; }       // overwrites the oldest
    if (r.kind == RowKind::Event)        { if (!events_.push(r)) dropped_++; }
    else if (r.kind == RowKind::Adxl375) { fast_.pushOverwrite(r); }   // oldest drops first
    else                                 { slow_.pushOverwrite(r); }
}

void Logger::service(uint32_t now_ms) {
    if (state_ != State::Recording) return;
    LogRow r;
    while (stageLen_ + kMaxRowBytes <= kStageSize) {
        if (!events_.pop(r) && !slow_.pop(r) && !fast_.pop(r)) break;
        stageLen_ += formatRow(r, stage_ + stageLen_, kStageSize - stageLen_);
    }
    if (stageLen_ >= 512) {
        size_t n = (stageLen_ / 512) * 512;                   // whole blocks only
        if (!sink_.write(stage_, n)) { retryOrFail(); return; }
        memmove(stage_, stage_ + n, stageLen_ - n);
        stageLen_ -= n;
    }
    if (now_ms - lastFlush_ >= kFlushMs) { sink_.flush(); lastFlush_ = now_ms; }
}
```

How the main loop uses it:

```cpp
void loop() {
    uint32_t now = micros();
    if (adxlDue(now)) logger.push(makeAdxlRow(now, adxl.read(), stateMachine));
    if (bmpDue(now))  logger.push(makeBmpRow(now, baro.read(), stateMachine));

    stateMachine.update();
    if (stateMachine.flightState() == FlightState::Boost && !logger.isRecording()) {
        logger.startRecording();
        logger.push(makeEventRow(now, EventId::LaunchFlag, stateMachine));
    }
    logger.service(millis());
    watchdogFeed();
}
```

## 6. Build steps

1. Agree the column list, event names, state codes, and the `LogRow` meaning with P1, P2, and P3 (shared interfaces, Week 3).
2. Laptop: `LogRow`, ring buffer, `ILogSink`, `MemorySink`, and `Logger` with `formatRow()`. Write tests LT1 to LT8, LT12, LT13, and LT14 first or alongside.
3. Laptop: a tiny program that pushes a synthetic 800 Hz and 25 Hz stream through the logger and writes a file. Hand that file to the plot script owner so the plot can be built before hardware exists.
4. Teensy: `SdSink` with card start, next free number, preallocation, header, flush, close. Blink and serial first, then a fixed stream of rows.
5. Teensy: replace the synthetic stream with the real sensor classes from P1 and the state machines from P2.
6. Bench: take the measurements in section 7 and run LT9 to LT11.
7. Decide CSV or lean text (open item L1) and set the queue sizes and flush interval (LOG.10).
8. Add recovery after a reset (LT8) once the nonvolatile state record from FLT.5 exists.
9. Write the README section: file names, the column list, and how to copy files off the card.

## 7. Measurements to take on the bench

Use the same class of card as the flight card (open item L7). Fill in the blank cells.

| Measurement | Why | Feeds | Result |
|-------------|-----|-------|--------|
| Worst case time for one 512 byte write over a 10 minute run | Sets the stall the queues must absorb | LOG.10, STO-4 | |
| Average write throughput at the full row rate | Compares with the data rate | STO-4, L1 | |
| Bytes per row for each kind in CSV mode, and per second at full rate | Sizes the buffers, decides L1 | L1 | |
| Same for lean text mode | Same | L1 | |
| Time of one `flush()` | Longest blocking call, sets the watchdog timeout | item 18, FLT.9 | |
| Time to open the card and preallocate the file | Pre flight time | LOG.4 | |
| Worst case duration of `push()` and of one `service()` pass | Proves bounded time | PLT-4, LT9 | |
| High water mark of each queue over a 10 minute run | Sets the queue sizes | LOG.10 | |
| Rows dropped over the run (must be zero in normal use) | Pass or fail | LOG.6 | |
| Pad wait duration to cover and the BMP390 averaging window | Sets the pre buffer lengths | LOG.5, L3 | |

## 8. Test plan

| ID | Test | Req | Where |
|----|------|-----|-------|
| LT1 | Header written once. Every row has the same number of columns. | FMT-4, LOG.9 | Laptop |
| LT2 | Every row has `sensor` and `t_us`. Time is monotonic within a kind. | FMT-1, FMT-5 | Laptop |
| LT3 | PAD IDLE rows appear in the file after `startRecording()`. | LOG.5 | Laptop |
| LT4 | A full fast queue drops the oldest acceleration rows, keeps every event, and the counter is right. | LOG.6 | Laptop |
| LT5 | A failing sink: `push()` still returns, the logger reports the failure, no hang. | ERR-2, LOG.6 | Laptop |
| LT6 | `close()` twice, and after a failure, is safe. | ERR-5 | Laptop |
| LT7 | Existing files are never overwritten. | STO-3 | Laptop |
| LT8 | After a simulated reset, a new file starts with `LOG_START` and `RESET_RECOVERED`, and time continues. | LOG.8 | Laptop |
| LT9 | `push()` and one `service()` pass stay under their time budget. | LOG.3, PLT-4 | Bench |
| LT10 | An 800 Hz stream for 10 minutes: no missed sensor reads, zero rows dropped. | STO-4 | Bench |
| LT11 | Pull the card mid run: the file is readable up to the last flush. | ERR-5 | Bench |
| LT12 | A sink that sleeps for the measured worst stall: queues absorb it without drops. | LOG.3, LOG.6 | Laptop |
| LT13 | Each injected fault shows its bit in `status` and its `FAULT` event. | LOG.7 | Laptop |
| LT14 | Recording starts at BOOST, stops at LANDED, and the file ends with `LOG_STOP`. | LOG.5, CTL-3 | Laptop |
| LT15 | The plot script opens a logger file without edits. | PST-1 | Laptop |
| LT16 | If lean text is built: the converted file equals the CSV mode output. | LOG.9 | Laptop |

## 9. Done when

* Tests LT1 to LT8, LT12 to LT15 pass on the laptop in the `native` PlatformIO environment.
* On the bench, an 800 Hz stream plus the 25 Hz stream runs for 10 minutes with no missed sensor reads and zero dropped rows (LT10), and `push()` and `service()` meet their time budget (LT9).
* Pulling the card mid run leaves a readable file (LT11).
* A simulated flight produces a file the plot script renders, with the right states, flags, controller fields, and actuator events.
* The measurement table is filled in, the CSV or lean text decision is written down, and the queue sizes and flush interval are set from it (LOG.10).
* LOG.8 to LOG.11 are added to the reefing doc and `REQUIREMENTS.md`.

## 10. Interfaces with other projects

| Project | Pushes or uses | Agreed in Week 3 |
|---------|----------------|------------------|
| P1 | `BMP390` and `ADXL375` rows, ground reference event | Units, sensor names |
| P2 | State and flag events, `APOGEE`, `CTRL` rows, backup timer events, the nonvolatile state record | State codes, event names, controller units |
| P3 | `ACT` rows, `ACT_CMD` events, rail and grant fields, motor driver faults | `act_state` codes |
| P4 plot, generator, harness | Reads the files. The generator writes the same format. | Column list, status word layout |

## 11. Risks

| Risk | Mitigation |
|------|------------|
| A slow SD write stalls the loop | Queues, whole block writes, bounded `service()`, LT9 and LT12 |
| CSV rows too large for the card or CPU | Measure first, lean text fallback with a converter (4.6) |
| Card quality varies, so bench results do not carry over | Test with the flight card class (L7) and keep a record of the card model |
| Last rows lost at power loss | Flush interval bounds the loss (L2) |
| A reset in flight splits the record | New file per reset, saved time base, stitch tool (LOG.8) |
| Row order differs from time order | Sort by `t_us` in the plot script |
| Format changes break the plot script | Fixed header and version event (LOG.9), change control through this file |
| No card fitted or card full on the pad | Report in pre flight status, flight logic unaffected, arming decision is item 22 |

## 12. Open items

| ID | Item | Needs |
|----|------|-------|
| L1 | CSV or lean text, decided by the measurements | P4A bench data |
| L2 | Flush interval, and how much data loss at power loss is acceptable | Software, Systems |
| L3 | Pre buffer lengths (pad wait and BMP390 averaging window) | P2, Systems |
| L4 | How often the elapsed flight time is saved for reset recovery (ties to FLT.5) | P2 |
| L5 | Wall clock in the file from the Teensy RTC, if a backup cell is fitted | Electrical |
| L6 | Status word bit layout (item 23 in `FAULT_HANDLING.md`) | Plot script owner |
| L7 | Card brand, size, format, and whether a card is fitted (LOG.4), and whether a missing card blocks arming (item 22) | Electrical |
| L8 | Units and sign convention of the controller columns (items 26 and 28) | Systems, Mechanical |
| L9 | `ACT` row rate and the `act_state` code table | P3 |

## 13. Schedule

| Week | Dates | Milestone |
|------|-------|-----------|
| 3 | Oct 26 to Nov 1 | Column list and event names agreed. Laptop logger, `MemorySink`, tests LT1 to LT8, LT12 to LT14. Synthetic sample file to the plot script owner. |
| 4 | Nov 2 to 8 | `SdSink` on the Teensy. First bench run with a synthetic stream. Measurements in section 7. Decide L1. |
| 5 | Nov 9 to 15 | LT9 to LT11, queue sizes and flush interval set, reset recovery. Review with the design review of the state machines. |
| 6 | Nov 16 to 22 | Merged into the single repo with the sensors, state machines, and controller (integration date 11/16). |
