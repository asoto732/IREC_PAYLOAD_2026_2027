#pragma once

#include <cstdint>

// Every BMP390 tunable in one place, one named constant each (SEN-6, DRV-8).
// Range limits match include/payload/Config.hpp in the main repo; keep them
// in sync rather than inventing new numbers (SEN-10).

namespace bmp390::config {

constexpr double kPressureMinKpa  =  30.0;   // BMP390: 300 hPa
constexpr double kPressureMaxKpa  = 125.0;   // BMP390: 1250 hPa
constexpr double kTemperatureMinC = -40.0;
constexpr double kTemperatureMaxC =  85.0;

// Same value as kOfflineAfterConsecutiveFaults in the main repo (DRV-7).
constexpr uint32_t kOfflineAfterConsecutiveFaults = 5;

// Register values written in begin(). ASSUMED per BMP-5 / BMP-6.
// TODO(BMP-5): confirm with the datasheet's conversion time formula that
// pressure x2 / temperature x1 fits inside 10 ms, and write the resulting
// conversion time in this comment: ______ us
constexpr uint8_t kOsrValue    = 0x01;  // osr_p = x2, osr_t = x1
constexpr uint8_t kOdrValue    = 0x01;  // 100 Hz (SEN-4)
constexpr uint8_t kConfigValue = 0x00;  // IIR filter off until BMP-6 is decided
constexpr uint8_t kPwrCtrlValue = 0x33; // press_en | temp_en | normal mode (BMP-4)

} // namespace bmp390::config
