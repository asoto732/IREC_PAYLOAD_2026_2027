#include "Barometer.hpp"
#include "Config.hpp"

bool Barometer::begin(TwoWire& wire) {
    // TODO(E32F-2, E32F-3): Start the sensor with
    //     bmp_.begin_I2C(cfg::kBmp390Address, &wire)
    // and return false if it fails.
    (void)wire;

    // TODO(E32F-8): Apply the three settings from Config.hpp with the
    // library's setter functions (look in Adafruit_BMP3XX.h for the names).

    return false;  // TODO: return true once the above succeeds
}

Reading Barometer::read() {
    Reading r;

    if (offline_) {
        r.pressureStatus = r.temperatureStatus = Status::SensorOffline;
        return r;
    }

    // TODO(E32F-22): Call bmp_.performReading(). If it returns false, set both
    // statuses to CommError, call registerFault(), and return r.

    // TODO(E32F-9): Copy bmp_.pressure and bmp_.temperature into r.
    // The library gives pressure in PASCALS; the file wants kPa.

    // TODO(E32F-10): Range check each value against Config.hpp. Out of range
    // means that value's status is OutOfRange (keep the value). Otherwise Ok.

    // TODO(E32F-23): If either status is not Ok, call registerFault();
    // otherwise call registerSuccess().

    return r;
}

// PROVIDED (E32F-23).
void Barometer::registerFault() {
    ++consecutiveFaults_;
    if (consecutiveFaults_ >= cfg::kOfflineAfterFaults) {
        offline_ = true;
    }
}
