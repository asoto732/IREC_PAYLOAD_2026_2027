#pragma once

// Copied unchanged from the main repo (include/payload/ISensor.hpp).
// Do not edit here: this prototype must use the same interface the Teensy
// flight code uses, so the finished driver ports over as is (DRV-1, PLT-3).

#include "payload/Types.hpp"

namespace payload {

class ISensor {
public:
    virtual ~ISensor() = default;

    // Identifies which column/stream this sensor writes to.
    virtual SensorId id() const = 0;

    // Attempt one reading. Returns true and sets `outValue` on success.
    // Returns false and sets `outStatus` to the reason on failure; the
    // caller is still expected to log the failed attempt (ERR-1, ERR-3).
    virtual bool read(double& outValue, StatusCode& outStatus) = 0;

    // True once the sensor has been marked offline (e.g. too many
    // consecutive faults) and should stop being polled (ERR-4).
    virtual bool isOffline() const = 0;
};

} // namespace payload
