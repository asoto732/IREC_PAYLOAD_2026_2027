#pragma once

#include <cstdint>
#include <Adafruit_BMP3XX.h>

// Every tunable value in one place, one named constant each (E32F-2, E32F-5,
// E32F-8). If you are typing a number into another file, it probably
// belongs here instead.

namespace cfg {

// ---- Wiring (E32F-2) ----
constexpr int      kSdaPin        = 13;
constexpr int      kSclPin        = 14;
constexpr uint32_t kI2cClockHz    = 400000;
constexpr uint8_t  kBmp390Address = 0x77;   // 0x76 if SDO is tied to GND
constexpr int      kBootButtonPin = 0;      // BOOT button, reads LOW when pressed

// ---- Sampling (E32F-5, E32F-6) ----
constexpr uint32_t kSampleRateHz   = 10;
constexpr uint32_t kSamplePeriodUs = 1000000UL / kSampleRateHz;

// ---- Sensor settings (E32F-8) ----
// TODO(E32F-8): Pick these on purpose. Look up each option in the BMP390
// datasheet (oversampling vs. noise vs. conversion time, and what the IIR
// filter does to a fast pressure change), then write one line under each
// saying why you chose it. The names come from bmp3_defs.h in the library.
constexpr uint8_t kPressureOversampling    = BMP3_NO_OVERSAMPLING;     // why: ______
constexpr uint8_t kTemperatureOversampling = BMP3_NO_OVERSAMPLING;     // why: ______
constexpr uint8_t kIirFilter               = BMP3_IIR_FILTER_DISABLE;  // why: ______

// ---- Valid ranges from the datasheet (E32F-10) ----
constexpr double kPressureMinKpa  =  30.0;
constexpr double kPressureMaxKpa  = 125.0;
constexpr double kTemperatureMinC = -40.0;
constexpr double kTemperatureMaxC =  85.0;

// ---- Faults (E32F-23) ----
constexpr uint32_t kOfflineAfterFaults = 5;

// ---- Storage (E32F-16, E32F-17, E32F-18) ----
constexpr const char* kLogPrefix       = "/log_";
constexpr const char* kLogSuffix       = ".txt";
constexpr uint32_t    kMaxLogFiles     = 1000;   // log_000 .. log_999
constexpr uint32_t    kFlushIntervalMs = 1000;
constexpr double      kMinFreeFraction = 0.10;

// ---- Run control (E32F-25) ----
constexpr uint32_t kMaxRunMs = 60000;

constexpr uint32_t kSerialBaud = 115200;

} // namespace cfg
