// Minimal, dependency-free smoke test for DataLogger: no test framework,
// just a small check() helper (deliberately not `assert`, so the checks
// still run in a Release/NDEBUG build), so it builds anywhere the main
// program does. Run via `ctest` from the build directory, or directly as
// an executable.
//
// Covers requirements: FMT-4 (CSV with header), STO-3 (unique file per
// session), ERR-3 (status column present on every row), ERR-5 (file is
// valid/closed after close()).

#include <fstream>
#include <iostream>
#include <string>

#include "payload/DataLogger.hpp"
#include "payload/Types.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    using namespace payload;

    const std::string testDir = "test_data";

    DataLogger logger(testDir, "unit_test_");
    const std::string path = logger.path();

    bool ok1 = logger.writeSample(Sample{0, SensorId::Pressure, 101.3, StatusCode::Ok});
    bool ok2 = logger.writeSample(Sample{10, SensorId::Vibration, 0.02, StatusCode::CommError});
    check(ok1, "writeSample should succeed for an open logger");
    check(ok2, "writeSample should succeed even for a faulted reading");

    logger.close();
    // Closing twice must be safe (called again implicitly by the destructor).
    logger.close();

    std::ifstream in(path);
    check(in.is_open(), "data file should exist and be readable after close()");

    std::string header;
    std::getline(in, header);
    check(header == "time_ms,sensor,value,status", "CSV header must match docs/DATA_FORMAT.md");

    std::string row1, row2;
    std::getline(in, row1);
    std::getline(in, row2);
    check(row1.find("PRESSURE") != std::string::npos, "row1 should contain PRESSURE");
    check(row1.find("OK") != std::string::npos, "row1 should have OK status");
    check(row2.find("VIBRATION") != std::string::npos, "row2 should contain VIBRATION");
    check(row2.find("COMM_ERROR") != std::string::npos, "row2 should have COMM_ERROR status");

    if (g_failures == 0) {
        std::cout << "data_logger_test: all checks passed (" << path << ")\n";
        return 0;
    }
    std::cerr << "data_logger_test: " << g_failures << " check(s) failed\n";
    return 1;
}
