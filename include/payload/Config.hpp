#pragma once

#include <cstdint>

// Central place for values that are still TBD pending Payload Electrical
// (see docs/REQUIREMENTS.md). Every value here is a single named constant
// so it can be changed without touching sampling/logging logic (SEN-6).
namespace payload::config {

// --- Sampling rates (SEN-4, SEN-5: ASSUMED, pending electrical) ---
constexpr double kPressureRateHz    = 1.0;
constexpr double kTemperatureRateHz = 1.0;
constexpr double kVibrationRateHz   = 100.0; // vibration samples much faster

constexpr std::uint32_t kPressurePeriodMs    = static_cast<std::uint32_t>(1000.0 / kPressureRateHz);
constexpr std::uint32_t kTemperaturePeriodMs = static_cast<std::uint32_t>(1000.0 / kTemperatureRateHz);
constexpr std::uint32_t kVibrationPeriodMs   = static_cast<std::uint32_t>(1000.0 / kVibrationRateHz);

// --- Storage (STO-2: ASSUMED stand-in until onboard storage is selected) ---
constexpr const char* kDataDirectory = "data";
constexpr const char* kFilenamePrefix = "payload_log_";

// --- Run control (CTL-4: ASSUMED ground/dev behavior) ---
// If no stop signal (Ctrl+C) arrives first, the program stops on its own
// after this many seconds. 0 means run until interrupted.
constexpr std::uint32_t kMaxRunSeconds = 30;

// --- Fault simulation, so the error-handling paths are exercised even
// with simulated sensors (ERR-1..ERR-5). Not a real requirement value,
// purely a demo knob. ---
constexpr double kSimulatedFaultProbability = 0.01; // per-sample chance of a transient fault
constexpr std::uint32_t kOfflineAfterConsecutiveFaults = 5; // faults in a row before a sensor is marked offline

} // namespace payload::config
