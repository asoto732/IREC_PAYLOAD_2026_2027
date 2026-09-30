#pragma once

#include <Adafruit_BMP3XX.h>
#include <Wire.h>
#include "Status.hpp"

// Wraps the Adafruit BMP3XX library (E32F-3) so the rest of the program only
// sees kPa, degrees C, and a status per value.
class Barometer {
public:
    // Connect to the BMP390 and apply the settings from Config.hpp.
    // Returns false if the sensor does not answer (E32F-21).
    bool begin(TwoWire& wire);

    // Take one reading of both pressure and temperature.
    Reading read();

    // True after too many failed reads in a row (E32F-23).
    bool isOffline() const { return offline_; }

private:
    void registerFault();
    void registerSuccess() { consecutiveFaults_ = 0; }

    Adafruit_BMP3XX bmp_;
    uint32_t consecutiveFaults_ = 0;
    bool offline_ = false;
};
