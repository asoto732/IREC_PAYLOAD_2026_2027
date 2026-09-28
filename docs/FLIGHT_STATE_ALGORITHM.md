# Flight State Algorithm (Sensor Suite)

Satisfies requirements FSD-1 through FSD-6 in `docs/REQUIREMENTS.md`. Written
for the parachute reefing PDR.

## Purpose

The existing sensor suite (`src/main.cpp`, `include/payload/*`) reads every
sensor at its own rate and logs it. It does not yet know what phase of
flight it is in. This algorithm adds that layer on top, without changing the
sampling loop or the `ISensor`/`DataLogger` interfaces (PLT-3 stays true):
it consumes the same pressure and vibration samples, derives altitude and
vertical velocity, and walks a small state machine from pad idle through
landing.

Two things depend on it:

* **CTL-3** (how recording starts and stops in flight) — launch detect and
  landed detect can start and stop the `DataLogger` directly, in place of
  the ground/dev Ctrl+C stand in (CTL-4). See FSD-6.
* **The reefing experiment** (`docs/REEFING_ALGORITHM.md`) — it needs to
  know when the main parachute has opened (reefed) so it can begin
  evaluating its own altitude thresholds. This algorithm is what hands it
  that starting signal.

## Inputs

| Signal | Source | Rate | Role here |
|---|---|---|---|
| Pressure | BMP390 | 100 Hz | The altitude source. Every state transition below is derived from it. |
| Temperature | BMP390 | 100 Hz | Optional compensation for the altitude formula. |
| Vibration | ADXL375 | 800 Hz | Launch detect (sustained high g), burnout detect (g drops), landed detect (g settles low). |

## Derived quantities

**Altitude above ground level.** During the first several seconds in
`PAD_IDLE`, average the pressure readings to get a ground reference
pressure, `groundPressureKpa`. After that, every pressure sample converts to
altitude with the standard barometric formula:

```
altitudeAgl = 44330.0 * (1.0 - pow(pressureKpa / groundPressureKpa, 1.0 / 5.255))
```

A temperature compensated version (using the hypsometric formula with the
BMP390's own temperature channel) is a reasonable upgrade once real hardware
is on the bench; the simple form above is enough to validate the state logic
in simulation.

**Filtered vertical velocity.** A raw derivative of a 100 Hz altitude signal
is noisy enough to false trigger apogee detection on its own, so the
velocity used for state transitions is smoothed with an exponential moving
average:

```
velocityRaw      = (altitudeAgl - previousAltitudeAgl) / dt
velocityFiltered = alpha * velocityRaw + (1 - alpha) * velocityFiltered
```

`alpha` (ASSUMED around 0.2) trades lag against noise rejection and should
be tuned against a simulated or real flight profile before PDR sign off.

## Flight states

| State | Meaning | Entry condition (debounced) | Exit / action |
|---|---|---|---|
| `PAD_IDLE` | On the pad, collecting the ground pressure reference. | Initial state. | Moves to `BOOST` once a sustained acceleration or altitude increase is seen. |
| `BOOST` | Powered ascent. | Vibration exceeds `kBoostAccelThreshold` for `kBoostDebounceSamples` consecutive samples. | Marks launch time, requests recording start (FSD-6). Moves to `COAST` once vibration drops (motor burnout) while still ascending. |
| `COAST` | Unpowered ascent. | Vibration below `kBurnoutVibrationThreshold` for `kBurnoutDebounceSamples`, `velocityFiltered > 0`. | Moves to `APOGEE` once `velocityFiltered` goes and stays negative for `kApogeeDebounceSamples`. |
| `APOGEE` | Momentary; records apogee altitude and time. **Observe only** — the payload does not command drogue deployment (REEF-8). | Reached from `COAST`. | Logs the apogee estimate for cross check against the recovery altimeter. Falls straight through to `DROGUE_DESCENT`. |
| `DROGUE_DESCENT` | Descending under drogue, watching for the recovery system to deploy the main. | Reached from `APOGEE`. | Moves to `MAIN_DESCENT` once `altitudeAgl <= kMainDeployAltM` and `velocityFiltered < 0`, confirmed for `kMainDeployDebounceSamples`. This is a **detection** of a deployment the recovery system commanded, not a command (REEF-8). Hands off to the reefing controller (`ReefingController::begin`). |
| `MAIN_DESCENT` | Descending under the main canopy while it steps from reefed to full open. | Reached from `DROGUE_DESCENT`. | The reefing controller runs its own three stage logic here (`docs/REEFING_ALGORITHM.md`); this state machine only watches for landing. |
| `LANDED` | On the ground. | `abs(altitudeAgl) < kLandedAltToleranceM`, `abs(velocityFiltered) < kLandedVelToleranceMs`, vibration below `kLandedVibrationThreshold`, all sustained for `kLandedDebounceSamples`. | Forces any unfired reef stage to fire immediately as a last safety net, then requests recording stop (CTL-2). Terminal. |

## State diagram

```
[PAD_IDLE]
    | sustained accel / altitude rise (debounced)
    v
[BOOST] --request recording start-->
    | vibration drops, still ascending (debounced)
    v
[COAST]
    | vertical velocity goes negative (debounced)
    v
[APOGEE] --(log the estimate only; the payload fires nothing, REEF-8)--
    | immediate
    v
[DROGUE_DESCENT]
    | altitude <= main deploy alt AND descending (debounced)
    v
[MAIN_DESCENT] <-----> [Reefing Stage 0/1/2, see docs/REEFING_ALGORITHM.md]
    | altitude ~0, velocity ~0, vibration low (debounced)
    v
[LANDED] --force any unfired reef stage, request recording stop-->
```

## Pseudocode

```
enum FlightState { PadIdle, Boost, Coast, Apogee, DrogueDescent, MainDescent, Landed }

state              = PadIdle
groundPressureKpa  = null
previousAltitudeAgl = 0
velocityFiltered   = 0
boostCount = burnoutCount = apogeeCount = mainDeployCount = landedCount = 0

function onPressureSample(timeUs, pressureKpa, status):
    if groundPressureKpa == null:
        accumulate pressureKpa into ground average
        if enough samples collected: groundPressureKpa = average
        return

    if status == OK:
        altitudeAgl = barometricAltitude(pressureKpa, groundPressureKpa)
        altitudeStatus = OK
    else:
        # FSD-5. With GPS removed there is no second measured altitude
        # source, so a faulted barometer drops straight to dead reckoning.
        altitudeAgl = timeBasedEstimate(timeUs) # TBD, needs a flight sim to seed
        altitudeStatus = TIMER_FALLBACK

    velocityRaw      = (altitudeAgl - previousAltitudeAgl) / dt
    velocityFiltered = alpha * velocityRaw + (1 - alpha) * velocityFiltered
    previousAltitudeAgl = altitudeAgl

    evaluateTransition(timeUs, altitudeAgl, velocityFiltered, altitudeStatus)

function evaluateTransition(timeUs, altitudeAgl, velocityFiltered, altitudeStatus):
    switch state:
        case PadIdle:
            if vibrationExceeds(kBoostAccelThreshold):
                boostCount += 1
                if boostCount >= kBoostDebounceSamples:
                    state = Boost
                    launchTimeUs = timeUs
                    logFlightState(Boost, altitudeStatus)
                    requestRecordingStart()          # FSD-6 / CTL-3
            else:
                boostCount = 0

        case Boost:
            if vibrationBelow(kBurnoutVibrationThreshold) and velocityFiltered > 0:
                burnoutCount += 1
                if burnoutCount >= kBurnoutDebounceSamples:
                    state = Coast
                    logFlightState(Coast, altitudeStatus)
            else:
                burnoutCount = 0

        case Coast:
            if velocityFiltered < 0:
                apogeeCount += 1
                if apogeeCount >= kApogeeDebounceSamples:
                    apogeeAltitude = altitudeAgl
                    logFlightState(Apogee, altitudeStatus)
                    # No deploy command here: the recovery system owns
                    # drogue and main (REEF-8). This is recorded only.
                    state = DrogueDescent
                    logFlightState(DrogueDescent, altitudeStatus)
            else:
                apogeeCount = 0

        case DrogueDescent:
            if altitudeAgl <= kMainDeployAltM and velocityFiltered < 0:
                mainDeployCount += 1
                if mainDeployCount >= kMainDeployDebounceSamples:
                    state = MainDescent
                    logFlightState(MainDescent, altitudeStatus)
                    reefingController.begin(timeUs)   # see REEFING_ALGORITHM.md
            else:
                mainDeployCount = 0

        case MainDescent:
            reefingController.update(timeUs, altitudeAgl, velocityFiltered)
            if abs(altitudeAgl) < kLandedAltToleranceM
               and abs(velocityFiltered) < kLandedVelToleranceMs
               and vibrationBelow(kLandedVibrationThreshold):
                landedCount += 1
                if landedCount >= kLandedDebounceSamples:
                    reefingController.forceRemainingStages(timeUs)  # safety net
                    state = Landed
                    logFlightState(Landed, altitudeStatus)
                    requestRecordingStop()            # CTL-2
            else:
                landedCount = 0

        case Landed:
            pass   # terminal
```

## Fault handling and redundancy (FSD-5)

**Decided: the BMP390 is the payload's only altitude source.** No GPS, no
second barometer. Flight states are detected from barometric data, every
disreef trigger is an altitude threshold, and the per stage backup timers
are the failsafe. That makes the barometer a single point of failure for
this algorithm, accepted knowingly, with the mitigations below.

* **Brief fault** — a pressure sample carries `OUT_OF_RANGE`,
  `COMM_ERROR`, or `SENSOR_OFFLINE`. Ride it out on the last good altitude:
  the debounce counters simply do not advance, so a dropped sample delays a
  transition rather than causing a false one. This is the common case and
  it is handled cleanly.
* **Sustained fault** — pressure unavailable long enough that state
  detection cannot continue. Fall back to an elapsed time estimate seeded
  from the nominal flight profile (expected time to apogee, expected
  descent rate). This is dead reckoning with no measurement behind it, and
  it stays TBD until a flight simulation gives real numbers to seed it.
* **The real failsafe is downstream.** If this algorithm cannot produce a
  trustworthy altitude, the disreef stages do not wait on it: each stage's
  backup timer in `docs/REEFING_ALGORITHM.md` (REEF-4) fires on elapsed
  time since the previous stage, and `forceRemainingStages()` drives any
  unfired stage at landing detection. A canopy recovered still reefed
  requires the barometer, both stage timers, *and* the landing safety net
  to all fail.
* Every transition is logged with which path produced it (`OK` or
  `TIMER_FALLBACK`) via the proposed `FLIGHT_STATE` row in
  `docs/DATA_FORMAT.md`, so a post flight review can see whether the
  barometric path held up the whole flight.

What this buys in exchange: one sensor, one bus, no cross checking logic,
and no ambiguity about which source a transition came from. What it costs
is that the timers have to be sized well — see the sizing note in the
reefing doc, which is now load bearing rather than belt and braces.

Two things protect the barometer in hardware: it gets an I2C bus to itself
(the SHT40 is on I2C1) so nothing else can stall it, and its bounds in
`include/payload/Config.hpp` come from the BMP390 datasheet so an
implausible reading is flagged rather than silently feeding the state
machine.

## Constants to pin down before PDR sign off

All of the following are ASSUMED placeholders so the logic above can be
simulated; none should be treated as flight values yet.

| Constant | Placeholder | Comes from |
|---|---|---|
| `kBoostAccelThreshold` | ~3 g sustained | Motor thrust curve. |
| `kBoostDebounceSamples` | 20 (at 800 Hz, ~25 ms) | Tuning against simulated launch data. |
| `kBurnoutVibrationThreshold` | ~1 g | Motor burn time / thrust curve. |
| `kApogeeDebounceSamples` | 10 (at 100 Hz, ~100 ms) | Tuning against simulated coast phase. |
| `alpha` (velocity EMA) | 0.2 | Noise vs. lag trade off, tune against simulated data. |
| `kMainDeployAltM` | 450 m AGL (~1500 ft) | Flight simulation + Recovery/Structures input (REEF-1). |
| `kMainDeployDebounceSamples` | 5 (at 100 Hz, 50 ms) | Same. |
| `kLandedAltToleranceM` | 3 m | Bench/drop test. |
| `kLandedVelToleranceMs` | 1 m/s | Bench/drop test. |
| `kLandedVibrationThreshold` | ~0.2 g | Bench/drop test. |
| `kLandedDebounceSamples` | 100 (at 100 Hz, 1 s) | Avoids declaring landed during a momentary bounce. |

## Open items for PDR

1. **How main deployment is actually sensed.** The payload no longer
   commands deployment (REEF-8), so `DROGUE_DESCENT` → `MAIN_DESCENT` is a
   detection, and right now it is an altitude threshold plus a descent
   check. That fires at the altitude the main *should* have opened at — it
   does not confirm the canopy actually opened. If the main fails or opens
   late, the reefing controller would start paying out line against a
   canopy that is not there. The direct evidence is the deceleration when
   the canopy inflates: a sharp drop in `velocityFiltered` (and a vibration
   spike on the ADXL375) within a window after the altitude crossing.
   Recommend adding that confirmation before flight; the altitude crossing
   alone is enough to simulate against but not enough to trust.
2. **Flight simulation is not available yet.** Every constant in the table
   above is a placeholder until it exists, and so are the reefing
   thresholds and timer windows. This is the single biggest schedule
   dependency for the algorithm work.
3. Whether temperature compensation is worth adding to the altitude formula
   given the BMP390 already supplies it on the same chip.
4. Bench and drop test plan to validate `PAD_IDLE`→`BOOST` and
   `MAIN_DESCENT`→`LANDED` debounce timing before trusting it to open/close
   the data file in flight (FSD-6).

## Decisions closed

* **FSD-5, altitude redundancy.** Single barometer accepted; the per stage
  backup timers are the failsafe. No second barometer, no GPS.
* **REEF-8, deployment ownership.** The payload commands nothing. `APOGEE`
  records an estimate, `DROGUE_DESCENT` → `MAIN_DESCENT` detects a
  deployment the recovery system made, and the payload's only actuation is
  the reefing motor after that point.
* **Pressure sensor.** BMP390 on I2C0, replacing the MS5611.
