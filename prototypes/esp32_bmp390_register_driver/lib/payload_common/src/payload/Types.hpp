#pragma once

// Copied unchanged from the main repo (include/payload/Types.hpp).

#include <cstdint>

namespace payload {

enum class SensorId {
    Pressure,
    Temperature,
    Vibration,
    Humidity
};

inline const char* toString(SensorId id) {
    switch (id) {
        case SensorId::Pressure:    return "PRESSURE";
        case SensorId::Temperature: return "TEMPERATURE";
        case SensorId::Vibration:   return "VIBRATION";
        case SensorId::Humidity:    return "HUMIDITY";
    }
    return "UNKNOWN";
}

enum class StatusCode {
    Ok = 0,
    Timeout = 1,
    OutOfRange = 2,
    CommError = 3,
    SensorOffline = 4,
    StorageError = 5
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

} // namespace payload
