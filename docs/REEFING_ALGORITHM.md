# Parachute Reefing Algorithm

Satisfies requirements REEF-1 through REEF-8 in `docs/REQUIREMENTS.md`.
Written for the parachute reefing PDR.

## Scope

The dual deploy, reefed main sequence: drogue
deploys at apogee, the main parachute deploys already reefed at altitude,
then steps open in three stages (reefed, one intermediate partial disreef,
full open) as it descends. The reefing mechanism itself is a motor driven
spool, not a cutter: a reefing line is sewn to the parachute and wound onto
a spool, and a gearbox plus a high torque motor pays the line out to let the
canopy open further. Each disreef stage is one commanded unwind of that
spool, so there are two motor triggered events total, both driving the same
actuator rather than two separate one shot devices.

**The payload does not fire the drogue or the initial reefed main
deployment** (REEF-8, decided). Those stay with the dedicated recovery
system, as competition and safety practice expect of an experiment payload.
This controller owns exactly two events: the two motor commanded disreef
steps after the main is already open reefed. That is the experiment —
proving out a staged, sensor triggered disreef — and the payload's only
actuation on the whole flight.

It is handed control by `docs/FLIGHT_STATE_ALGORITHM.md` when that state
machine detects the main has opened (`DROGUE_DESCENT` → `MAIN_DESCENT`), and
it hands back landing safety duty to that same state machine.

## Stages

| Stage | Canopy state | Trigger | Actuation |
|---|---|---|---|
| `Reefed` | Main open, spool still fully wound in (reefing line at its packed length). | Entry state — set by `begin()` when the flight state algorithm detects main deployment. No motor command yet. | none |
| `PartialDisreef` | Motor unwinds the spool to the stage 1 line length; canopy steps to an intermediate area. | Altitude AGL ≤ `kDisreef1AltM`, confirmed descending, debounced (REEF-3); or backup timer (REEF-4). | Reefing motor: unwind to stage 1 position |
| `FullOpen` | Motor unwinds the spool the rest of the way; canopy reaches its full area. | Altitude AGL ≤ `kDisreef2AltM`, confirmed descending, debounced; or backup timer. | Reefing motor: unwind to stage 2 (full) position |

Stages only move forward and only one at a time (REEF-5): `PartialDisreef`
is never evaluated until `Reefed` has been entered, and `FullOpen` is never
evaluated until `PartialDisreef` has actually been reached. This protects
against a noisy altitude reading near one threshold accidentally commanding
a later stage out of order — worth being careful about here since it is the
same motor and spool driving both stages, not two independent devices.

## Stage diagram

```
 main opens reefed (from Flight State Algorithm)
            |
            v
       [ Reefed ]                spool fully wound in
            |  altitude <= kDisreef1AltM, descending, debounced
            |  --- or backup timer kDisreef1TimeoutS elapsed ---
            v
   motor unwinds spool to Stage 1 length
            |
            v
   [ PartialDisreef ]            spool partly paid out
            |  altitude <= kDisreef2AltM, descending, debounced
            |  --- or backup timer kDisreef2TimeoutS elapsed ---
            v
   motor unwinds spool to Stage 2 (full) length
            |
            v
      [ FullOpen ]  (terminal)   spool fully paid out

 Safety net: if the Flight State Algorithm reaches LANDED while any stage
 above has not been reached, it calls forceRemainingStages() to drive the
 motor to whatever position is left immediately, so the canopy is never
 recovered still reefed.
```

Altitude wise, the three thresholds form a simple descending ladder:

```
kMainDeployAltM   ----  main opens reefed (recovery system fires it; payload only senses it)
kDisreef1AltM     ----  motor unwinds to Stage 1: Reefed -> PartialDisreef
kDisreef2AltM     ----  motor unwinds to Stage 2 (full): PartialDisreef -> FullOpen
   ground         ----  landed
```

## Pseudocode

```
enum ReefStage { None, Reefed, PartialDisreef, FullOpen }

stage             = None
stageEntryTimeUs  = 0
disreef1Count     = 0
disreef2Count     = 0

function begin(timeUs):
    stage = Reefed
    stageEntryTimeUs = timeUs
    logReefStage(Reefed, OK)   # records when the reefed main was observed open

function update(timeUs, altitudeAgl, velocityFiltered):
    if stage == None:
        return   # not handed off yet

    if stage == Reefed:
        evaluateStage(timeUs, altitudeAgl, velocityFiltered,
                      thresholdAlt: kDisreef1AltM,
                      timeoutS:     kDisreef1TimeoutS,
                      counter:      disreef1Count,     # passed by reference
                      nextStage:    PartialDisreef)

    else if stage == PartialDisreef:
        evaluateStage(timeUs, altitudeAgl, velocityFiltered,
                      thresholdAlt: kDisreef2AltM,
                      timeoutS:     kDisreef2TimeoutS,
                      counter:      disreef2Count,
                      nextStage:    FullOpen)

    # FullOpen: terminal, nothing left to evaluate

function evaluateStage(timeUs, altitudeAgl, velocityFiltered,
                        thresholdAlt, timeoutS, counter, nextStage):
    elapsedS = (timeUs - stageEntryTimeUs) / 1_000_000.0

    if altitudeAgl <= thresholdAlt and velocityFiltered < 0:
        counter += 1
        if counter >= kReefDebounceSamples:
            triggerStage(timeUs, nextStage, triggerStatus: OK)
            return
    else:
        counter = 0   # reset on any sample that does not confirm the crossing

    if elapsedS >= timeoutS:
        triggerStage(timeUs, nextStage, triggerStatus: TIMER_FALLBACK)  # REEF-4

function triggerStage(timeUs, nextStage, triggerStatus):
    reached = driveSpoolToStage(nextStage, kUnwindStallTimeoutMs)
    status = reached ? triggerStatus : MOTOR_FAULT
    logReefStage(nextStage, status)     # REEF-6, proposed DATA_FORMAT.md row
    stage = nextStage
    stageEntryTimeUs = timeUs

function forceRemainingStages(timeUs):
    # Called by the Flight State Algorithm on LANDED, as a last resort so
    # the canopy is never recovered still reefed even if both the altitude
    # trigger and the backup timer for a stage somehow never fired.
    if stage == Reefed:
        triggerStage(timeUs, PartialDisreef, TIMER_FALLBACK)
        triggerStage(timeUs, FullOpen,       TIMER_FALLBACK)
    else if stage == PartialDisreef:
        triggerStage(timeUs, FullOpen, TIMER_FALLBACK)
```

## Why a debounce and a descent check on top of the raw threshold

A single sample dipping below an altitude threshold is not enough to
command the motor: the BMP390 has measurement noise, and a momentary dip
while still essentially level could otherwise trigger a stage early.
`kReefDebounceSamples` consecutive samples below the threshold, combined
with requiring `velocityFiltered < 0` (genuinely descending, not just
noisy), is the same pattern used for apogee and landing detection in
`docs/FLIGHT_STATE_ALGORITHM.md`, so the whole system reasons about noise
one way.

## Why a backup timer per stage (REEF-4)

The altitude trigger depends on the pressure sensor and the shared ground
reference from the Flight State Algorithm. If that path is faulted for this
stage (`OUT_OF_RANGE`, `COMM_ERROR`, `SENSOR_OFFLINE`) the debounce condition
may never satisfy, and a canopy stuck reefed all the way to the ground is a
hard failure for the experiment and a safety issue for whatever the payload
is attached to.

**These timers are the failsafe, by decision, not a belt and braces
addition.** The BMP390 is the payload's only altitude source — no GPS, no
second barometer (FSD-5 in `docs/FLIGHT_STATE_ALGORITHM.md`). A failed
barometer goes straight to these timers plus the landed safety net, with no
degraded but still measured mode in between. Two consequences:

* Size each `kDisreefNTimeoutS` as a window that a healthy altitude trigger
  will always beat, but that still fires well above the ground under the
  slowest plausible descent for that canopy configuration. Too tight and it
  pre empts a working barometer; too loose and it fires too low to matter.
  This sizing needs the flight simulation.
* Treat the timers as flight critical in test, not as a rarely exercised
  path. Bench testing should include a run with the barometer deliberately
  failed, confirming both stages fire on time and the landing safety net
  catches anything left. Each stage's `kDisreefNTimeoutS` is sized from the expected
descent rate under that stage's canopy configuration plus margin, so the
timer fires only if the altitude path is clearly not working, not as a
race against it.

## Motor actuation and confirmation

`driveSpoolToStage()` commands the reefing motor, through its gearbox, to
unwind the spool to the target reefing line length for the next stage — the
sewn line paying out under motor control rather than a one shot release.
The actual motor, gearbox ratio, and how the target position is confirmed
(encoder count, a limit switch at each stage, or motor current/stall
sensing) are Payload Electrical/Mechanical decisions (REEF-7). If the motor
does not reach the target position within `kUnwindStallTimeoutMs`, that
trigger should log `MOTOR_FAULT` rather than `OK`, so post flight analysis
can tell a clean disreef from one where the spool may not have actually
reached the target length.

## Constants to pin down before PDR sign off

All ASSUMED, consistent with `docs/FLIGHT_STATE_ALGORITHM.md`'s constants
table, pending a flight simulation and mechanical testing of the reefing
hardware:

| Constant | Placeholder | Comes from |
|---|---|---|
| `kDisreef1AltM` | 300 m AGL | Flight simulation + Recovery/Structures (REEF-1, REEF-7). |
| `kDisreef2AltM` | 150 m AGL | Same, must be below `kDisreef1AltM` with margin. |
| `kReefDebounceSamples` | 5 (at 100 Hz, 50 ms) | Same debounce reasoning as Flight State Algorithm. |
| `kDisreef1TimeoutS` | ~12 s after `Reefed` entry | Expected descent rate under the reefed configuration, plus margin. |
| `kDisreef2TimeoutS` | ~12 s after `PartialDisreef` entry | Expected descent rate under the partial configuration, plus margin. |
| `kUnwindStallTimeoutMs` | TBD | Reefing motor/gearbox datasheet and bench testing, once selected. |

## Open items for PDR

1. **Reefing motor, gearbox, and spool selection**, and how the target
   position is confirmed — encoder, limit switch, or stall current sensing.
   This decides whether `MOTOR_FAULT` can ever actually be detected, or is
   only inferred from a missing confirmation signal. Largest hardware
   unknown now that the sensor set is settled.
2. **The two altitude thresholds and two timeout windows**, once a flight
   simulation gives real descent rates for the reefed and partially
   disreefed configurations. The simulation does not exist yet, so every
   number in the constants table above is a placeholder.
3. The actual reefing line length released at each stage, and whether two
   disreef steps are enough to meet the experiment's descent rate / shock
   goals, or a third step is warranted.
4. Confirm with Recovery/Structures whether commanding both remaining
   stages back to back in `forceRemainingStages()` (the landed safety net)
   is safe for the spool and motor, or whether it needs a minimum spacing
   even in that fallback case.
5. How `begin()` knows the main actually opened. It is currently handed off
   on an altitude threshold plus a descent check, which fires where the
   main *should* have opened rather than confirming that it did — see the
   open item in `docs/FLIGHT_STATE_ALGORITHM.md`. Paying out reefing line
   against a canopy that never opened is the failure mode to rule out.

## Decisions closed

* **REEF-2, actuation.** Motor driven spool with two commanded unwind
  positions. Not line cutters — one actuator, two positions, which is why
  stage ordering and stall detection matter here.
* **REEF-8, deployment ownership.** The payload does not command drogue or
  main deployment; it senses that the main opened and runs the disreef
  stages from there.
* **FSD-5, altitude source.** Barometric altitude from the BMP390 only.
  Every disreef trigger is an altitude threshold, and these stages' backup
  timers are the accepted failsafe.
