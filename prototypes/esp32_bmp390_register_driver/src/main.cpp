// BMP390 on a Freenove ESP32 WROVER: prototype of the flight driver.
//
// Prints one CSV row per reading over Serial, in the same format the flight
// logger writes (docs/DATA_FORMAT.md): time_us,sensor,value,status
// Open the monitor with `pio device monitor`.

#include <Arduino.h>
#include <Wire.h>

#include "ArduinoI2cBus.hpp"
#include "BoardConfig.hpp"
#include "Bmp390.hpp"

static ArduinoI2cBus i2cBus(Wire, board::kBmp390Address);
static bmp390::Bmp390 bmp(i2cBus);

static uint32_t recordingStartUs = 0;
static uint32_t nextSampleUs = 0;

// PROVIDED: one CSV row per reading (FMT-1, FMT-3, FMT-7: 7 significant digits).
static void logReading(payload::ISensor& sensor) {
    double value = 0.0;
    payload::StatusCode status = payload::StatusCode::Ok;
    sensor.read(value, status);
    Serial.printf("%lu,%s,%.7g,%s\n",
                  static_cast<unsigned long>(micros() - recordingStartUs),
                  payload::toString(sensor.id()), value,
                  payload::toString(status));
}

void setup() {
    Serial.begin(board::kSerialBaud);
    delay(500);  // give the USB serial monitor a moment to attach

    // TODO(BMP-1, DRV-5): Start Wire on board::kI2cSdaPin / kI2cSclPin at
    // board::kI2cClockHz, then set the bus timeout with
    // Wire.setTimeOut(board::kI2cTimeoutMs).

    // TODO(DRV-3): Call bmp.begin(). If it fails, print a clear message
    // (wiring? address?) and retry once a second. Do not continue until it
    // succeeds.

    Serial.println("time_us,sensor,value,status");
    recordingStartUs = micros();
    nextSampleUs = recordingStartUs;
}

void loop() {
    // TODO(SEN-4): Run the block below once every board::kSamplePeriodUs.
    // Use micros() and nextSampleUs, and advance nextSampleUs by the period
    // each time (do not reset it to micros(), or the rate slowly drifts).
    // Do not use delay() here (DRV-4).
    {
        const uint32_t t0 = micros();
        bmp.update();
        const uint32_t elapsed = micros() - t0;

        // PROVIDED: DRV-4 check. If this ever prints, update() is too slow.
        if (elapsed > board::kReadBudgetUs) {
            Serial.printf("# WARN update() took %lu us (budget %lu)\n",
                          static_cast<unsigned long>(elapsed),
                          static_cast<unsigned long>(board::kReadBudgetUs));
        }

        logReading(bmp.pressure());
        logReading(bmp.temperature());
    }
}
