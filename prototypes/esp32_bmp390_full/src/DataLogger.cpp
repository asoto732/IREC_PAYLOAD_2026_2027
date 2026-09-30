#include "DataLogger.hpp"
#include "Config.hpp"

bool DataLogger::mount() {
    // TODO(E32F-4, E32F-19): Mount with LittleFS.begin(true). The `true`
    // formats the partition the very first time, when it is still blank.
    // Return whether it worked.
    return false;
}

// Build "/log_000.txt", "/log_001.txt", ... and return the first one that
// does not exist yet (E32F-16).
String DataLogger::nextFreePath() const {
    // TODO(E32F-16): Loop i from 0 to cfg::kMaxLogFiles - 1. Build the name
    // with snprintf(buf, sizeof(buf), "%s%03lu%s", cfg::kLogPrefix,
    // (unsigned long)i, cfg::kLogSuffix). Return the first one for which
    // LittleFS.exists(buf) is false. Return "" if every name is taken.
    return "";
}

bool DataLogger::open() {
    // TODO(E32F-16): path_ = nextFreePath(); fail if it is empty.

    // TODO(E32F-11): Open it for writing: LittleFS.open(path_, FILE_WRITE).
    // A File converts to false if opening failed.

    // TODO(E32F-12): Write the header line "time_us,sensor,value,status".

    // TODO: Set isOpen_ = true and lastFlushMs_ = millis(), then return true.
    return false;
}

bool DataLogger::writeLine(uint32_t timeUs, const char* sensor, double value, Status status) {
    if (!isOpen_) {
        return false;
    }
    // TODO(E32F-12, E32F-14): Write one line with file_.printf(...):
    //     "%lu,%s,%.7g,%s\n"
    // using (unsigned long)timeUs, sensor, value, toString(status).
    // TODO(E32F-20): printf returns the number of bytes written. Zero means
    // the write failed: return false.
    (void)timeUs;
    (void)sensor;
    (void)value;
    (void)status;
    return false;
}

void DataLogger::maybeFlush() {
    // TODO(E32F-17): If isOpen_ and at least cfg::kFlushIntervalMs have
    // passed since lastFlushMs_, call file_.flush() and update lastFlushMs_.
    // Compare with subtraction (millis() - lastFlushMs_ >= interval) so it
    // still works when millis() wraps around.
}

bool DataLogger::hasSpace() const {
    // TODO(E32F-18): Free fraction = 1 - usedBytes / totalBytes, both from
    // LittleFS. Return whether it is above cfg::kMinFreeFraction.
    // Use double math, not integer division.
    return true;
}

// PROVIDED.
void DataLogger::close() {
    if (isOpen_) {
        file_.flush();
        file_.close();
        isOpen_ = false;
    }
}
