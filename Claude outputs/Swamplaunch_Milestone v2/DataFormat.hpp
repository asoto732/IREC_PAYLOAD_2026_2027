#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace payload {
inline constexpr unsigned kFormatVersion = 1;
inline constexpr std::size_t kEventBytes = 128;
inline constexpr const char* kCsvHeader =
    "t_us,sensor,flight_state,reef_state,status,alt_m,press_pa,temp_c,"
    "ax_g,ay_g,az_g,act_state,grants,motor_pos,motor_a,rail12_v,rail24_v,"
    "tgt_rate,meas_rate,drag,err,cmd_width,meas_width,event";

enum class RowKind : std::uint8_t { Bmp390, Adxl375, Act, Ctrl, Event };
enum class FlightState : unsigned {
    PadIdle, Boost, Coast, Descent, Deploy, MainDescent, Landed
};
struct LogRow {
    std::uint64_t t_us = 0; // Caller supplies measurement time, not write time.
    RowKind kind = RowKind::Bmp390;
    unsigned flight_state = 0;
    unsigned reef_state = 0;
    std::uint32_t status = 0;
    // BMP: altitude m, pressure Pa, temperature C. ADXL: x/y/z in g.
    // ACT: state, grants, encoder counts, current A, 12 V rail, 24 V rail.
    // CTRL: target rate, measured rate, drag, error, commanded/measured width.
    std::array<double, 6> values{};
    bool has_motor_position = false;
    const char* event = ""; // Copied in push(); caller may then reuse the text.
};
}
