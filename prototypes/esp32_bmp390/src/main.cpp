// ESP32 + BMP390 logger
//
// Reads pressure and temperature 10 times a second for 60 seconds and saves
// every reading to /log.txt in the ESP32's flash. Type d in the serial
// monitor afterward to print the file.
//
// Requirements: REQUIREMENTS.md. Each TODO names the requirement it meets.
// Guides that teach each piece: PROJECT_SPEC.md, Part 3 and Part 4.

#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>
#include <Adafruit_BMP3XX.h>

// ---------------------------------------------------------------- settings
// One named constant per setting. No magic numbers further down.

const int      SDA_PIN     = 13;      // E32-2
const int      SCL_PIN     = 14;      // E32-2
const uint8_t  BMP_ADDRESS = 0x77;    // E32-2 (0x76 if SDO is tied to GND)

const uint32_t SAMPLE_PERIOD_US  = 100000;  // 10 Hz (E32-5)
const uint32_t RUN_TIME_MS       = 60000;   // stop after 60 s (E32-10)
const uint32_t FLUSH_INTERVAL_MS = 1000;    // save to flash every second (E32-11)

const char*    LOG_PATH = "/log.txt";       // E32-7

// ------------------------------------------------------------------ state

Adafruit_BMP3XX bmp;
File logFile;

bool     recording     = false;
uint32_t startUs       = 0;   // micros() when recording started
uint32_t startMs       = 0;   // millis() when recording started
uint32_t nextSampleUs  = 0;   // when the next reading is due
uint32_t lastFlushMs   = 0;   // when the file was last saved to flash

// ------------------------------------------------------ provided helpers

// Write one line of the log: time_us,sensor,value,status (E32-8).
// %.7g keeps 7 significant digits, enough for 0.1 Pa on ~101 kPa.
void writeLine(uint32_t timeUs, const char* sensor, double value, const char* status) {
    logFile.printf("%lu,%s,%.7g,%s\n", (unsigned long)timeUs, sensor, value, status);
}

// Save and close the file. The only way recording ends (E32-10).
void stopRecording() {
    logFile.flush();
    logFile.close();
    recording = false;
    Serial.println("Recording stopped. Type d to print the file.");
}

// Print the whole log between two marker lines, so it can be copied into a
// .txt on your laptop (E32-12).
void dumpLog() {
    File f = LittleFS.open(LOG_PATH, FILE_READ);
    if (!f) {
        Serial.println("No log file yet.");
        return;
    }
    Serial.printf("BEGIN FILE %s\n", LOG_PATH);
    while (f.available()) {
        Serial.write(f.read());
    }
    Serial.println("END FILE");
    f.close();
}

// ------------------------------------------------------------------ setup

void setup() {
    Serial.begin(115200);
    delay(500);  // give the serial monitor a moment to attach
    Serial.println("ESP32 BMP390 logger");

    // TODO(E32-2): Start I2C on our pins.
    //     Wire.begin(SDA_PIN, SCL_PIN);

    // TODO(E32-3): Start the sensor with bmp.begin_I2C(BMP_ADDRESS, &Wire).
    // If it returns false, print "BMP390 not found, check wiring", wait one
    // second, and try again. Keep trying until it works.
    // (A while loop around begin_I2C does this.)

    // PROVIDED: sensor settings. More oversampling = less noise but slower.
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_DISABLE);

    // TODO(E32-4): Mount the filesystem with LittleFS.begin(true).
    // If it returns false, print "LittleFS mount failed" and return.

    // TODO(E32-7): Open the log for writing, which replaces any old file:
    //     logFile = LittleFS.open(LOG_PATH, FILE_WRITE);
    // If (!logFile), print "Could not open log file" and return.

    // TODO(E32-8): Write the header line to the file:
    //     time_us,sensor,value,status

    // PROVIDED: start the clocks.
    startUs      = micros();
    startMs      = millis();
    nextSampleUs = startUs;
    lastFlushMs  = startMs;
    recording    = true;
    Serial.println("Recording to /log.txt for 60 s...");
}

// ------------------------------------------------------------------- loop

void loop() {
    // PROVIDED: d prints the file, once recording is over.
    if (Serial.available() && Serial.read() == 'd') {
        if (recording) {
            Serial.println("Still recording. Wait for it to finish.");
        } else {
            dumpLog();
        }
    }

    if (!recording) {
        return;
    }

    // TODO(E32-10): If millis() - startMs >= RUN_TIME_MS, call
    // stopRecording() and return.

    // TODO(E32-5): Only read when it is time. If micros() has not reached
    // nextSampleUs yet, return. Otherwise move nextSampleUs forward by
    // SAMPLE_PERIOD_US. Do not use delay() here.
    //
    //     if ((int32_t)(micros() - nextSampleUs) < 0) return;
    //     nextSampleUs += SAMPLE_PERIOD_US;
    //
    // (Why the subtraction? micros() wraps back to 0 every ~71 minutes;
    // subtracting still gives the right answer when it does.)

    const uint32_t t = micros() - startUs;  // time since recording started

    // TODO(E32-6, E32-9): Take a reading with bmp.performReading().
    //   * If it returns true, write two lines with writeLine():
    //       PRESSURE    bmp.pressure / 1000.0   (library gives Pa, we want kPa)
    //       TEMPERATURE bmp.temperature         (already in C)
    //     both with status "OK".
    //   * If it returns false, still write both lines, with value 0 and
    //     status "COMM_ERROR". Never skip a line just because the read failed.
    (void)t;  // delete this line once you use t

    // TODO(E32-11): If millis() - lastFlushMs >= FLUSH_INTERVAL_MS, call
    // logFile.flush() and set lastFlushMs = millis(). Flushing makes sure a
    // reset or unplugged cable loses at most about one second of data.
}
