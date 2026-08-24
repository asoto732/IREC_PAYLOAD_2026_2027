// Payload Software -- initial data collection framework (simulated inputs).
//
// Reads a simulated pressure, temperature, and vibration sensor, each at
// its own defined sampling rate (vibration faster than the other two, per
// the assignment), timestamps every reading, and writes it to a CSV file.
// Detects per-sensor faults and storage faults, keeps logging from the
// sensors that are still healthy if one goes offline, and always leaves the
// data file safely closed -- on a normal stop, a max-runtime stop, or
// Ctrl+C. See docs/REQUIREMENTS.md for the requirement each behavior here
// satisfies.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "payload/Config.hpp"
#include "payload/DataLogger.hpp"
#include "payload/ISensor.hpp"
#include "payload/SimulatedSensors.hpp"

namespace {

// Set by the signal handler only; read by the main loop. A signal handler
// must not do real work (no I/O, no locks), so this is the entire handler.
std::atomic<bool> g_stopRequested{false};

void handleSigint(int /*signum*/) {
    g_stopRequested.store(true, std::memory_order_relaxed);
}

struct ScheduledSensor {
    payload::ISensor* sensor;
    std::uint32_t periodMs;
    std::uint64_t nextDueMs;
};

} // namespace

int main() {
    using namespace payload;
    using Clock = std::chrono::steady_clock;

    std::signal(SIGINT, handleSigint);

    std::cout << "Payload Software -- data collection (simulated sensors)\n";
    std::cout << "Sampling: pressure " << config::kPressureRateHz << " Hz, "
              << "temperature " << config::kTemperatureRateHz << " Hz, "
              << "vibration " << config::kVibrationRateHz << " Hz\n";

    PressureSensor pressure;
    TemperatureSensor temperature;
    VibrationSensor vibration;

    std::unique_ptr<DataLogger> logger;
    try {
        logger = std::make_unique<DataLogger>(config::kDataDirectory, config::kFilenamePrefix);
    } catch (const std::exception& ex) {
        // Can't open the data file at all -- report and exit (ERR-2).
        std::cerr << "FATAL: " << ex.what() << "\n";
        return 1;
    }
    std::cout << "Logging to " << logger->path() << "\n";
    std::cout << "Recording started. Press Ctrl+C to stop safely";
    if (config::kMaxRunSeconds > 0) {
        std::cout << " (auto-stops after " << config::kMaxRunSeconds << "s)";
    }
    std::cout << ".\n";

    std::vector<ScheduledSensor> scheduled = {
        {&pressure,    config::kPressurePeriodMs,    0},
        {&temperature, config::kTemperaturePeriodMs, 0},
        {&vibration,   config::kVibrationPeriodMs,   0},
    };

    const auto startTime = Clock::now();
    auto elapsedMs = [&]() -> std::uint64_t {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
    };

    std::size_t sampleCount = 0;
    std::size_t errorCount = 0;
    bool anySensorOnline = true;

    while (!g_stopRequested.load(std::memory_order_relaxed) && anySensorOnline) {
        const std::uint64_t now = elapsedMs();

        if (config::kMaxRunSeconds > 0 && now >= static_cast<std::uint64_t>(config::kMaxRunSeconds) * 1000) {
            std::cout << "Max run duration reached, stopping.\n";
            break;
        }

        anySensorOnline = false;
        for (auto& entry : scheduled) {
            if (entry.sensor->isOffline()) {
                continue; // ERR-4: keep logging the sensors that still work
            }
            anySensorOnline = true;

            if (now < entry.nextDueMs) {
                continue;
            }

            double value = 0.0;
            StatusCode status = StatusCode::Ok;
            bool ok = entry.sensor->read(value, status);
            if (!ok) {
                ++errorCount;
                if (status == StatusCode::SensorOffline) {
                    std::cerr << "WARNING: " << toString(entry.sensor->id())
                              << " sensor went offline after repeated faults; "
                              << "continuing with remaining sensors.\n";
                }
            }

            Sample sample{now, entry.sensor->id(), value, status};
            if (!logger->writeSample(sample)) {
                std::cerr << "FATAL: storage write failed, closing file safely.\n";
                logger->close();
                return 2; // ERR-2 / STO write failure -- nothing more we can safely do
            }
            ++sampleCount;

            entry.nextDueMs += entry.periodMs;
            if (entry.nextDueMs <= now) {
                // We fell behind (e.g. slow tick); resync instead of
                // free-running a burst of catch-up samples.
                entry.nextDueMs = now + entry.periodMs;
            }
        }

        // Sleep until the next fastest sensor is due, at minimum a couple
        // of milliseconds so the loop is not spinning the CPU.
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (!anySensorOnline) {
        std::cerr << "All sensors offline; ending recording.\n";
    }

    // Safe shutdown path (ERR-5 / CTL-2), reached on stop request, max
    // runtime, or all sensors going offline. Also runs implicitly via
    // ~DataLogger if we returned early above.
    logger->close();

    std::cout << "Recording stopped. Samples written: " << sampleCount
              << ", faulted reads: " << errorCount << "\n";
    std::cout << "Data file: " << logger->path() << "\n";

    return 0;
}
