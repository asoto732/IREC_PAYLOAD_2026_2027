#pragma once

#include <cstdint>

// Central place for the values that describe the selected flight hardware
// (see docs/HARDWARE.md) plus the few items still pending Payload
// Electrical (see docs/REQUIREMENTS.md). Every value here is a single named
// constant so it can be changed without touching sampling/logging logic
// (SEN-6).
namespace payload::config {

// --- Sampling rates (SEN-4, SEN-5, SEN-8, SEN-9) ---
// Set from the selected parts: the BMP390 supplies both pressure and
// temperature and goes to 200 Hz ODR, so 100 Hz leaves headroom; the
// ADXL375 has ~1 kHz of bandwidth so 800 Hz stays inside it; and the SHT40
// measures a slow-moving quantity.
constexpr double kPressureRateHz    = 100.0; // BMP390 (200 Hz ODR available)
constexpr double kTemperatureRateHz = 100.0; // BMP390 (same chip as pressure)
constexpr double kVibrationRateHz   = 800.0; // ADXL375
constexpr double kHumidityRateHz    = 1.0;   // SHT40

// Periods are microseconds, not milliseconds: 800 Hz is a 1.25 ms period,
// which integer milliseconds cannot express (it would round to 1 ms and
// silently run the ADXL375 at 1 kHz).
constexpr std::uint64_t kPressurePeriodUs    = static_cast<std::uint64_t>(1'000'000.0 / kPressureRateHz);
constexpr std::uint64_t kTemperaturePeriodUs = static_cast<std::uint64_t>(1'000'000.0 / kTemperatureRateHz);
constexpr std::uint64_t kVibrationPeriodUs   = static_cast<std::uint64_t>(1'000'000.0 / kVibrationRateHz);
constexpr std::uint64_t kHumidityPeriodUs    = static_cast<std::uint64_t>(1'000'000.0 / kHumidityRateHz);

// A desktop OS routinely overshoots a requested sleep by a whole scheduler
// tick (about 1-15 ms on Windows), which at 800 Hz would quietly drop most
// of the vibration stream. The scheduler therefore sleeps only down to
// this margin and spins out the remainder. The Teensy 4.1 has an accurate
// microsecond timer and will not need the spin -- this is a development
// host accommodation, not flight behavior.
constexpr std::uint64_t kSchedulerSpinMarginUs = 2000;

// --- Datasheet limits (SEN-7) ---
// Readings outside these bounds are recorded as OUT_OF_RANGE rather than
// dropped (ERR-1). Sourced from the selected parts, not guessed.
constexpr double kPressureMinKpa    =  30.0;   // BMP390: 300 hPa
constexpr double kPressureMaxKpa    = 125.0;   // BMP390: 1250 hPa
constexpr double kTemperatureMinC   = -40.0;   // BMP390 operating range
constexpr double kTemperatureMaxC   =  85.0;
constexpr double kVibrationMinG     = -200.0;  // ADXL375 full scale
constexpr double kVibrationMaxG     =  200.0;
constexpr double kHumidityMinPct    =   0.0;   // SHT40
constexpr double kHumidityMaxPct    = 100.0;

// --- Storage (STO-1: microSD in the Teensy 4.1's built-in slot) ---
// On the flight target this is the SD card mount; on a development machine
// it is just a local directory, which is why it stays a constant.
constexpr const char* kDataDirectory = "data";
constexpr const char* kFilenamePrefix = "payload_log_";

// Flushing every single row was affordable at 100 Hz; at 800 Hz it is not,
// and the electronics research calls out slow SD writes as a flight risk.
// Rows are buffered and flushed on this interval instead, which bounds how
// much data a brownout can cost us (STO-4, ERR-5).
constexpr std::uint32_t kFlushIntervalMs = 250;

// --- Run control (CTL-4: ASSUMED ground/dev behavior; the real flight
// trigger is still TBD, see CTL-3) ---
// If no stop signal (Ctrl+C) arrives first, the program stops on its own
// after this many seconds. 0 means run until interrupted.
constexpr std::uint32_t kMaxRunSeconds = 30;

// --- Fault simulation, so the error-handling paths are exercised even
// with simulated sensors (ERR-1..ERR-5). Not a real requirement value,
// purely a demo knob. ---
constexpr double kSimulatedFaultProbability = 0.001; // per-sample chance of a transient fault
constexpr std::uint32_t kOfflineAfterConsecutiveFaults = 5; // faults in a row before a sensor is marked offline

} // namespace payload::config
