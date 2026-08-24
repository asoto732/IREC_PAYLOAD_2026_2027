#pragma once

#include "payload/Types.hpp"

namespace payload {

// Every sensor -- simulated today, a real driver later -- implements this.
// Keeping the interface this small is what lets the simulated
// implementations be swapped for real hardware drivers without touching
// the scheduler or the logger (PLT-3).
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
