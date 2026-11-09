#pragma once
#include "DataFormat.hpp"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace payload {
inline bool validRow(const LogRow& row) {
    if (row.flight_state > 6 || row.reef_state > 3) return false;
    if (row.kind == RowKind::Event) {
        if (!row.event) return false;
        for (std::size_t i = 0; i < kEventBytes; ++i) {
            const char ch = row.event[i];
            if (ch == '\0') return i != 0;
            if (ch == ',' || ch == '\r' || ch == '\n' || ch == '"') return false;
        }
        return false;
    }
    std::size_t count = 0;
    switch (row.kind) {
        case RowKind::Bmp390: case RowKind::Adxl375: count = 3; break;
        case RowKind::Act: case RowKind::Ctrl: count = 6; break;
        default: return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (row.kind == RowKind::Act && i == 2 && !row.has_motor_position) continue;
        if (!std::isfinite(row.values[i])) return false;
    }
    if (row.kind == RowKind::Act) {
        const auto integer = [](double n) { return n == std::trunc(n); };
        if (!integer(row.values[0]) || row.values[0] < 0 || row.values[0] > 255 ||
            !integer(row.values[1]) || row.values[1] < 0 || row.values[1] > 7) return false;
        if (row.has_motor_position && (!integer(row.values[2]) ||
            row.values[2] < -2147483648.0 || row.values[2] > 2147483647.0)) return false;
    }
    return true;
}

// Allocations and formatting occur only in service(), never push().
inline std::string formatRow(const LogRow& row) {
    if (!validRow(row)) return {};
    std::array<std::string, 24> cells{};
    cells[0] = std::to_string(row.t_us);
    cells[2] = std::to_string(row.flight_state);
    cells[3] = std::to_string(row.reef_state);
    std::ostringstream status;
    status.imbue(std::locale::classic());
    status << "0x" << std::hex << std::uppercase << row.status;
    cells[4] = status.str();
    if (row.kind == RowKind::Event) {
        cells[1] = "EVENT";
        cells[23] = row.event;
    } else {
        std::size_t first = 0, count = 0;
        switch (row.kind) {
            case RowKind::Bmp390: cells[1] = "BMP390"; first = 5; count = 3; break;
            case RowKind::Adxl375: cells[1] = "ADXL375"; first = 8; count = 3; break;
            case RowKind::Act: cells[1] = "ACT"; first = 11; count = 6; break;
            case RowKind::Ctrl: cells[1] = "CTRL"; first = 17; count = 6; break;
            default: return {};
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (row.kind == RowKind::Act && i == 2 && !row.has_motor_position) continue;
            if (row.kind == RowKind::Act && i < 3) {
                cells[first + i] = std::to_string(static_cast<std::int64_t>(row.values[i]));
                continue;
            }
            std::ostringstream number;
            number.imbue(std::locale::classic());
            number << std::setprecision(9) << row.values[i];
            cells[first + i] = number.str();
        }
    }
    std::string line;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i) line += ',';
        line += cells[i];
    }
    return line + '\n';
}
}
