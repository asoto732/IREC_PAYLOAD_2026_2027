#pragma once

namespace payload::format {

// Single source of truth for the CSV header so the writer (DataLogger) and
// any documentation stay in sync (FMT-3, FMT-4, FMT-5).
constexpr const char* kCsvHeader = "time_ms,sensor,value,status";

} // namespace payload::format
