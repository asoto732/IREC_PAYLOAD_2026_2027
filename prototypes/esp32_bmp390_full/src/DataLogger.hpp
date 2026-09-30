#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "Status.hpp"

// Owns the .txt file in LittleFS (E32F-4, E32F-11 to E32F-20).
class DataLogger {
public:
    // Mount LittleFS. Returns false (and the caller reports it) on failure.
    bool mount();

    // Create the next unused /log_NNN.txt and write the header line.
    bool open();

    // Append one line: time_us,sensor,value,status. Returns false on a
    // failed write (E32F-20).
    bool writeLine(uint32_t timeUs, const char* sensor, double value, Status status);

    // Flush if kFlushIntervalMs has passed since the last flush (E32F-17).
    void maybeFlush();

    // True while free space is above kMinFreeFraction (E32F-18).
    bool hasSpace() const;

    // Flush and close. Safe to call more than once.
    void close();

    bool isOpen() const { return isOpen_; }
    const String& path() const { return path_; }

private:
    String nextFreePath() const;

    File file_;
    String path_;
    bool isOpen_ = false;
    uint32_t lastFlushMs_ = 0;
};
