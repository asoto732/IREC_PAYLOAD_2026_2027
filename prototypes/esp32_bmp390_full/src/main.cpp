// ESP32 + BMP390 logger
// Reads pressure and temperature and saves them to /log_NNN.txt in LittleFS.
// Requirements: REQUIREMENTS.md. Every TODO names the requirement it meets.

#include <Arduino.h>
#include <Wire.h>

#include "Barometer.hpp"
#include "Commands.hpp"
#include "Config.hpp"
#include "DataLogger.hpp"

static Barometer barometer;
static DataLogger logger;

static bool recording = false;
static uint32_t recordingStartUs = 0;
static uint32_t recordingStartMs = 0;
static uint32_t nextSampleUs = 0;

// PROVIDED: the one way recording ever stops, so the file is always closed
// the same way (E32F-25).
static void stopRecording(const char* reason) {
    if (!recording) {
        return;
    }
    logger.close();
    recording = false;
    Serial.printf("Recording stopped (%s). File: %s\n", reason, logger.path().c_str());
}

// PROVIDED: write one line and report a failed write (E32F-20).
static void writeValue(uint32_t timeUs, const char* sensor, double value, Status status) {
    if (!logger.writeLine(timeUs, sensor, value, status)) {
        Serial.printf("STORAGE_ERROR writing %s at %lu us\n", sensor,
                      static_cast<unsigned long>(timeUs));
    }
}

void setup() {
    Serial.begin(cfg::kSerialBaud);
    delay(500);  // let the serial monitor attach
    Serial.println("ESP32 BMP390 logger");

    pinMode(cfg::kBootButtonPin, INPUT_PULLUP);

    // TODO(E32F-2): Start I2C on the configured pins and clock:
    //     Wire.begin(cfg::kSdaPin, cfg::kSclPin, cfg::kI2cClockHz);

    // TODO(E32F-19): Mount the filesystem with logger.mount(). On failure,
    // print a clear message and stop here (return), without recording.

    // TODO(E32F-21): Call barometer.begin(Wire) until it succeeds. After
    // each failure print "BMP390 not found: check wiring and address" and
    // wait one second. (delay() is fine here; nothing is being sampled yet.)

    // TODO(E32F-24): Open the log file with logger.open(). If that fails,
    // report it and return. Otherwise set recording = true, set both
    // recordingStart values (micros() and millis()), set nextSampleUs to
    // recordingStartUs, and print which file is being written.
}

void loop() {
    bool stopRequested = false;
    handleSerialCommands(recording, stopRequested);

    if (!recording) {
        return;
    }

    // TODO(E32F-25): Stop conditions. Call stopRecording("<reason>") and
    // return if any of these is true:
    //   * stopRequested (someone typed x)
    //   * the BOOT button reads LOW
    //   * millis() - recordingStartMs >= cfg::kMaxRunMs
    //   * !logger.hasSpace()                      (E32F-18)
    (void)stopRequested;

    // TODO(E32F-7): Only sample when it is time. If
    // (int32_t)(micros() - nextSampleUs) < 0, return. Otherwise advance
    // nextSampleUs += cfg::kSamplePeriodUs (advance it, do not reset it to
    // micros(), or the rate slowly drifts). No delay() in here.

    const Reading r = barometer.read();
    const uint32_t t = micros() - recordingStartUs;  // E32F-13

    // TODO(E32F-12): Write two lines with writeValue(), one "PRESSURE" and
    // one "TEMPERATURE", both with timestamp t.
    (void)r;
    (void)t;

    // TODO(E32F-23): If barometer.isOffline() is now true, write one
    // SENSOR_OFFLINE line for each sensor, then stopRecording("sensor offline").

    logger.maybeFlush();  // E32F-17
}
