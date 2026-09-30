#pragma once

#include <cstdint>

// Board specific settings for the Freenove ESP32 WROVER prototype.
// Only this file and ArduinoI2cBus change when the driver moves to the
// Teensy 4.1 (Wire on pins 18/19 there, see docs/HARDWARE.md).

namespace board {

// GPIO 13 / 14 are the I2C pins Freenove's own tutorials use on this board,
// and they stay clear of the camera connector pins (21, 22 and others).
constexpr int kI2cSdaPin = 13;
constexpr int kI2cSclPin = 14;

constexpr uint32_t kI2cClockHz   = 400000;  // BMP-1: 400 kHz
constexpr uint16_t kI2cTimeoutMs = 1;       // DRV-5: ASSUMED 1 ms

// 0x77 is the Adafruit breakout default. 0x76 if SDO is tied to GND.
constexpr uint8_t kBmp390Address = 0x77;

constexpr uint32_t kSamplePeriodUs = 10000;  // 100 Hz (SEN-4)
constexpr uint32_t kReadBudgetUs   = 400;    // DRV-4 budget for one update()

constexpr uint32_t kSerialBaud = 115200;

} // namespace board
