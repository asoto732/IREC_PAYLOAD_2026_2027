#include "payload/DataLogger.hpp"
#include "payload/DataFormat.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

namespace payload {

namespace {

std::string makeTimestampedFilename(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    gmtime_s(&tmBuf, &t);
#else
    gmtime_r(&t, &tmBuf);
#endif

    std::ostringstream oss;
    oss << prefix << std::put_time(&tmBuf, "%Y%m%dT%H%M%SZ") << ".csv";
    return oss.str();
}

void ensureDirectoryExists(const std::string& directory) {
    // Minimal, dependency-free directory creation. mkdir() returning EEXIST
    // is not an error for our purposes. Windows takes no mode argument,
    // which matters because development happens on both.
#if defined(_WIN32)
    _mkdir(directory.c_str());
#else
    mkdir(directory.c_str(), 0755);
#endif
}

} // namespace

DataLogger::DataLogger(const std::string& directory, const std::string& filenamePrefix,
                       std::uint32_t flushIntervalMs)
    : flushIntervalMs_(flushIntervalMs) {
    ensureDirectoryExists(directory);
    path_ = directory + "/" + makeTimestampedFilename(filenamePrefix);

    file_.open(path_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("DataLogger: failed to open data file at " + path_);
    }

    // The stream default of 6 significant digits quantizes a ~101.3 kPa
    // reading to 0.001 kPa (1 Pa), which is coarser than the BMP390's
    // +/-3 Pa relative accuracy. 7 digits gives 0.1 Pa granularity, so the
    // barometer's precision survives the write -- and every flight state
    // and reefing trigger is derived from this stream.
    file_ << std::setprecision(7);

    file_ << format::kCsvHeader << "\n";
    file_.flush();
    lastFlush_ = Clock::now();
}

DataLogger::~DataLogger() {
    close();
}

bool DataLogger::writeSample(const Sample& sample) {
    if (closed_ || !file_.is_open()) {
        return false;
    }

    file_ << sample.timeUs << ','
          << toString(sample.sensor) << ','
          << sample.value << ','
          << toString(sample.status) << '\n';

    if (!file_.good()) {
        return false;
    }

    // Flushing guarantees that if power is lost or the sensor dies
    // mid-flight, everything written so far is actually on the card rather
    // than sitting in a buffer (ERR-5). Doing it per row costs an SD write
    // per sample, which the 800 Hz vibration stream cannot afford, so the
    // flight configuration flushes on an interval and accepts losing at
    // most that window (STO-4, and the "slow SD writes" risk in
    // docs/HARDWARE.md).
    if (flushIntervalMs_ == 0) {
        file_.flush();
        return file_.good();
    }

    const auto now = Clock::now();
    const auto sinceFlush =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlush_).count();
    if (sinceFlush >= static_cast<long long>(flushIntervalMs_)) {
        file_.flush();
        lastFlush_ = now;
        return file_.good();
    }

    return true;
}

void DataLogger::close() {
    if (closed_) {
        return;
    }
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
    closed_ = true;
}

} // namespace payload
