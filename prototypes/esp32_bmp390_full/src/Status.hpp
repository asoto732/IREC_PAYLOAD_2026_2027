#pragma once

// Per line status codes (E32F-15). PROVIDED.

enum class Status {
    Ok,
    OutOfRange,
    CommError,
    SensorOffline,
    StorageError
};

inline const char* toString(Status s) {
    switch (s) {
        case Status::Ok:            return "OK";
        case Status::OutOfRange:    return "OUT_OF_RANGE";
        case Status::CommError:     return "COMM_ERROR";
        case Status::SensorOffline: return "SENSOR_OFFLINE";
        case Status::StorageError:  return "STORAGE_ERROR";
    }
    return "UNKNOWN";
}

// One sample: both values come from the same performReading() call, so they
// share a timestamp (E32F-12).
struct Reading {
    double pressureKpa    = 0.0;
    double temperatureC   = 0.0;
    Status pressureStatus    = Status::CommError;
    Status temperatureStatus = Status::CommError;
};
