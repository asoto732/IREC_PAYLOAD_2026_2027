#pragma once

#include <fstream>
#include <string>
#include "payload/Types.hpp"

namespace payload {

// Owns the saved-data CSV file end to end: creating a uniquely named file
// per session (STO-3), writing the header, appending rows, and making sure
// the file is always flushed and closed safely -- on normal stop, on a
// sensor going offline, or on a caught interrupt signal (ERR-5, CTL-2).
class DataLogger {
public:
    // Creates `directory` if needed and opens a new file named
    // "<prefix><UTC-ish timestamp>.csv" inside it. Throws std::runtime_error
    // if the file cannot be opened (surfaced as a storage error, ERR-2).
    DataLogger(const std::string& directory, const std::string& filenamePrefix);

    // Safe shutdown happens here too, so an early return or exception
    // unwinding the stack still leaves the file correctly closed (ERR-5).
    ~DataLogger();

    DataLogger(const DataLogger&) = delete;
    DataLogger& operator=(const DataLogger&) = delete;

    // Appends one row. Returns false if the write failed (ERR-2); the
    // caller decides how to react (e.g. attempt a safe shutdown).
    bool writeSample(const Sample& sample);

    // Flushes and closes the file. Safe to call more than once. Called
    // automatically by the destructor if not called explicitly.
    void close();

    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::ofstream file_;
    bool closed_ = false;
};

} // namespace payload
