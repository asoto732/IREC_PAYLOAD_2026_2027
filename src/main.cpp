// Payload Software -- data collection framework (simulated inputs).
//
// Reads the selected payload sensor set -- BMP390 pressure/temperature,
// ADXL375 vibration, and SHT40 humidity -- each at its own defined
// sampling rate, timestamps every reading off one shared clock, and
// writes it to a CSV file. Detects
// per-sensor faults and storage faults, keeps logging from the sensors that
// are still healthy if one goes offline, and always leaves the data file
// safely closed -- on a normal stop, a max-runtime stop, or Ctrl+C.
//
// The sensors are still simulated: the parts are selected (see
// docs/HARDWARE.md) but the drivers are fall work. Rates, ranges, and the
// storage budget here all come from the selected hardware. See
// docs/REQUIREMENTS.md for the requirement each behavior satisfies.

#include <algorithm>
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
    std::uint64_t periodUs;
    std::uint64_t nextDueUs;
};

} // namespace

int main() {
    using namespace payload;
    using Clock = std::chrono::steady_clock;

    std::signal(SIGINT, handleSigint);

    std::cout << "Payload Software -- data collection (simulated sensors)\n";
    std::cout << "Sampling: pressure " << config::kPressureRateHz << " Hz, "
              << "temperature " << config::kTemperatureRateHz << " Hz, "
              << "vibration " << config::kVibrationRateHz << " Hz, "
              << "humidity " << config::kHumidityRateHz << " Hz\n";

    PressureSensor pressure;        // BMP390
    TemperatureSensor temperature;  // BMP390
    VibrationSensor vibration;      // ADXL375
    HumiditySensor humidity;        // SHT40

    std::unique_ptr<DataLogger> logger;
    try {
        logger = std::make_unique<DataLogger>(config::kDataDirectory,
                                              config::kFilenamePrefix,
                                              config::kFlushIntervalMs);
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
        {&pressure,    config::kPressurePeriodUs,    0},
        {&temperature, config::kTemperaturePeriodUs, 0},
        {&vibration,   config::kVibrationPeriodUs,   0},
        {&humidity,    config::kHumidityPeriodUs,    0},
    };

    const auto startTime = Clock::now();
    auto elapsedUs = [&]() -> std::uint64_t {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - startTime).count());
    };

    std::size_t sampleCount = 0;
    std::size_t errorCount = 0;
    bool anySensorOnline = true;

    while (!g_stopRequested.load(std::memory_order_relaxed) && anySensorOnline) {
        const std::uint64_t now = elapsedUs();

        if (config::kMaxRunSeconds > 0 &&
            now >= static_cast<std::uint64_t>(config::kMaxRunSeconds) * 1'000'000ULL) {
            std::cout << "Max run duration reached, stopping.\n";
            break;
        }

        anySensorOnline = false;
        std::uint64_t nextWakeUs = now + 1'000'000ULL; // no sensor due within a second: idle

        for (auto& entry : scheduled) {
            if (entry.sensor->isOffline()) {
                continue; // ERR-4: keep logging the sensors that still work
            }
            anySensorOnline = true;

            if (now < entry.nextDueUs) {
                nextWakeUs = std::min(nextWakeUs, entry.nextDueUs);
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

            entry.nextDueUs += entry.periodUs;
            if (entry.nextDueUs <= now) {
                // We fell behind (e.g. a slow SD write); resync instead of
                // free-running a burst of catch-up samples.
                entry.nextDueUs = now + entry.periodUs;
            }
            nextWakeUs = std::min(nextWakeUs, entry.nextDueUs);
        }

        // Wait until the soonest sensor is actually due rather than
        // ticking on a fixed interval: at 800 Hz the vibration slot is
        // 1.25 ms wide, so a fixed 2 ms tick would quietly halve the
        // ADXL375's rate. Sleep for the bulk of the wait, then spin out
        // the last couple of milliseconds, because sleep_for on a desktop
        // OS overshoots by a full scheduler tick (see
        // config::kSchedulerSpinMarginUs).
        const std::uint64_t afterPass = elapsedUs();
        if (nextWakeUs > afterPass) {
            const std::uint64_t remainingUs = nextWakeUs - afterPass;
            if (remainingUs > config::kSchedulerSpinMarginUs) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(remainingUs - config::kSchedulerSpinMarginUs));
            }
            while (elapsedUs() < nextWakeUs &&
                   !g_stopRequested.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
        }
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
