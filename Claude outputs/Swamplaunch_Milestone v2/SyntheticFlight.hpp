#pragma once
#include "Logger.hpp"
#include <cmath>
#include <stdexcept>

namespace payload {
inline constexpr std::uint64_t kPadDurationUs = 1000000;
inline constexpr std::uint64_t kAdxlPeriodUs = 1250, kBmpPeriodUs = 40000;

// Virtual time: one second on the pad, then a synthetic flight lasting seconds.
// The owner calls begin() first. All lifecycle transitions are explicit here.
inline void runSyntheticFlight(Logger& logger, unsigned seconds = 10) {
    if (seconds == 0 || seconds > 3600) throw std::invalid_argument("Duration must be 1..3600 seconds");
    const auto require = [](bool ok) { if (!ok) throw std::runtime_error("Synthetic logger operation failed"); };
    const std::uint64_t duration = static_cast<std::uint64_t>(seconds) * 1000000;
    const std::uint64_t end = kPadDurationUs + duration;
    LogRow row;
    const auto event = [&](std::uint64_t t, unsigned state, const char* text) {
        LogRow e; e.t_us = t; e.kind = RowKind::Event; e.flight_state = state;
        e.reef_state = state == 6 ? 3 : 0; e.event = text;
        require(logger.push(e));
    };
    event(0, 0, "META:GROUND_REF_PA:101325");
    event(0, 0, "STATE:PAD_IDLE");
    unsigned previous = 0;
    constexpr const char* states[] = {"STATE:PAD_IDLE", "STATE:BOOST", "STATE:COAST",
        "STATE:DESCENT", "STATE:DEPLOY", "STATE:MAIN_DESCENT", "STATE:LANDED"};
    for (std::uint64_t t = 0; t < end; t += kAdxlPeriodUs) {
        const double phase = t < kPadDurationUs ? 0.0 :
            static_cast<double>(t - kPadDurationUs) / static_cast<double>(duration);
        const unsigned state = t < kPadDurationUs ? 0 : phase < 0.2 ? 1 :
            phase < 0.5 ? 2 : phase < 0.6 ? 3 : phase < 0.7 ? 4 : 5;
        if (state != previous) {
            if (state == 1) require(logger.startRecording(t));
            event(t, state, states[state]);
            if (state == 1) event(t, state, "FLAG:LAUNCH");
            if (state == 3) {
                event(t, state, "FLAG:APOGEE");
                event(t, state, "APOGEE:120");
            }
            previous = state;
        }
        const double altitude = state == 0 ? 0 : 120 * std::sin(3.141592653589793 * phase);
        row.t_us = t; row.flight_state = state;
        row.kind = RowKind::Adxl375;
        row.values = {0.01 * std::sin(phase * 20), -0.02,
            state == 1 ? 1 + 8 * std::sin(phase * 15.70796326794897) : 1.0};
        require(logger.push(row));
        if (t % kBmpPeriodUs == 0) {
            row.kind = RowKind::Bmp390;
            row.values = {altitude, 101325 * std::pow(1 - altitude / 44330, 5.255),
                          22.5 - altitude * 0.0065};
            require(logger.push(row));
        }
        logger.service(static_cast<std::uint32_t>(t / 1000));
        require(!logger.storageFailed());
    }
    event(end, 6, states[6]);
    logger.close();
    for (unsigned pass = 0; pass < 10000 && !logger.isClosed() && !logger.storageFailed(); ++pass)
        logger.service(static_cast<std::uint32_t>(end / 1000));
    require(logger.isClosed() && logger.droppedRows() == 0);
}
}
