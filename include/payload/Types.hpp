#pragma once

#include <cstdint>
#include <string>

namespace payload {

// One row of saved data corresponds to one sensor's reading at one instant.
// See docs/DATA_FORMAT.md for the reasoning (SEN-3 / FMT-5: vibration
// samples much faster than pressure/temperature, so a single fixed-width
// row per timestamp would waste space and complicate partial failures).
enum class SensorId {
    Pressure,
    Temperature,
    Vibration
};

inline const char* toString(SensorId id) {
    switch (id) {
        case SensorId::Pressure:    return "PRESSURE";
        case SensorId::Temperature: return "TEMPERATURE";
        case SensorId::Vibration:   return "VIBRATION";
    }
    return "UNKNOWN";
}

// Per-row status/error indicator (requirement ERR-3). A row is never
// dropped just because something went wrong -- the row is written with a
// status code so post-flight analysis can tell good data from faulted data.
enum class StatusCode {
    Ok = 0,
    Timeout = 1,        // sensor did not respond in time
    OutOfRange = 2,      // reading outside physically plausible bounds
    CommError = 3,       // simulated bus/comm fault
    SensorOffline = 4,   // sensor has been marked offline after repeated faults
    StorageError = 5     // write to the data file failed
};

inline const char* toString(StatusCode code) {
    switch (code) {
        case StatusCode::Ok:            return "OK";
        case StatusCode::Timeout:       return "TIMEOUT";
        case StatusCode::OutOfRange:    return "OUT_OF_RANGE";
        case StatusCode::CommError:     return "COMM_ERROR";
        case StatusCode::SensorOffline: return "SENSOR_OFFLINE";
        case StatusCode::StorageError:  return "STORAGE_ERROR";
    }
    return "UNKNOWN";
}

// A single timestamped measurement, ready to be written as one CSV row.
struct Sample {
    std::uint64_t timeMs;   // elapsed milliseconds since recording start (FMT-2)
    SensorId sensor;
    double value;
    StatusCode status;
};

} // namespace payload
