#include "payload/DataLogger.hpp"
#include "payload/DataFormat.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

namespace payload {

namespace {

std::string makeTimestampedFilename(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
    gmtime_r(&t, &tmBuf);

    std::ostringstream oss;
    oss << prefix << std::put_time(&tmBuf, "%Y%m%dT%H%M%SZ") << ".csv";
    return oss.str();
}

void ensureDirectoryExists(const std::string& directory) {
    // Minimal, dependency-free directory creation. mkdir() returning EEXIST
    // is not an error for our purposes.
    mkdir(directory.c_str(), 0755);
}

} // namespace

DataLogger::DataLogger(const std::string& directory, const std::string& filenamePrefix) {
    ensureDirectoryExists(directory);
    path_ = directory + "/" + makeTimestampedFilename(filenamePrefix);

    file_.open(path_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("DataLogger: failed to open data file at " + path_);
    }

    file_ << format::kCsvHeader << "\n";
    file_.flush();
}

DataLogger::~DataLogger() {
    close();
}

bool DataLogger::writeSample(const Sample& sample) {
    if (closed_ || !file_.is_open()) {
        return false;
    }

    file_ << sample.timeMs << ','
          << toString(sample.sensor) << ','
          << sample.value << ','
          << toString(sample.status) << '\n';

    if (!file_.good()) {
        return false;
    }

    // Flushing every row costs some throughput but guarantees that if power
    // is lost or the sensor dies mid-flight, everything written so far is
    // actually on disk rather than sitting in a buffer (ERR-5, STO related).
    file_.flush();
    return file_.good();
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
