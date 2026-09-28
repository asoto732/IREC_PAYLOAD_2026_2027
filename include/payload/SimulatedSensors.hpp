#pragma once

#include <random>
#include "payload/ISensor.hpp"
#include "payload/Config.hpp"

namespace payload {

// Software stand-ins for the selected flight hardware (docs/HARDWARE.md).
// The parts are chosen but not yet in hand, so each class below simulates
// the stream its real chip will produce, using that chip's datasheet range
// for the out-of-range bounds. Swapping in a real driver means writing a
// new ISensor implementation, not touching the sampling loop (PLT-3).

// Shared behavior for all simulated sensors: a slow random walk around a
// realistic baseline, plus an injected fault rate so the error-handling
// requirements (ERR-1..ERR-5) are actually exercised during development,
// before real hardware and real faults exist.
class SimulatedSensorBase : public ISensor {
public:
    SimulatedSensorBase(SensorId id, double baseline, double noiseStdDev,
                         double lowerBound, double upperBound, unsigned seed,
                         double reversionRate = 0.05)
        : id_(id),
          baseline_(baseline),
          value_(baseline),
          lowerBound_(lowerBound),
          upperBound_(upperBound),
          reversionRate_(reversionRate),
          rng_(seed),
          noiseDist_(0.0, noiseStdDev),
          faultDist_(0.0, 1.0) {}

    SensorId id() const override { return id_; }
    bool isOffline() const override { return offline_; }

    bool read(double& outValue, StatusCode& outStatus) override {
        if (offline_) {
            outStatus = StatusCode::SensorOffline;
            return false;
        }

        // Simulated communication fault, independent of the value itself.
        if (faultDist_(rng_) < config::kSimulatedFaultProbability) {
            registerFault();
            outStatus = StatusCode::CommError;
            return false;
        }

        // Mean-reverting random walk: real pressure/temperature/vibration
        // readings hover around a baseline rather than drifting away
        // forever, so pull gently back toward it each sample.
        value_ += noiseDist_(rng_) - reversionRate_ * (value_ - baseline_);

        if (value_ < lowerBound_ || value_ > upperBound_) {
            registerFault();
            outStatus = StatusCode::OutOfRange;
            // Still report the offending value so it is visible in the log.
            outValue = value_;
            return false;
        }

        consecutiveFaults_ = 0;
        outValue = value_;
        outStatus = StatusCode::Ok;
        return true;
    }

private:
    void registerFault() {
        ++consecutiveFaults_;
        if (consecutiveFaults_ >= config::kOfflineAfterConsecutiveFaults) {
            offline_ = true;
        }
    }

    SensorId id_;
    double baseline_;
    double value_;
    double lowerBound_;
    double upperBound_;
    double reversionRate_;
    bool offline_ = false;
    std::uint32_t consecutiveFaults_ = 0;

    std::mt19937 rng_;
    std::normal_distribution<double> noiseDist_;
    std::uniform_real_distribution<double> faultDist_;
};

// BMP390 (pressure half). Bounds are the datasheet 300-1250 hPa range.
class PressureSensor : public SimulatedSensorBase {
public:
    explicit PressureSensor(unsigned seed = 1)
        : SimulatedSensorBase(SensorId::Pressure, /*baseline kPa*/ 101.3,
                               /*noise*/ 0.05,
                               config::kPressureMinKpa, config::kPressureMaxKpa, seed) {}
};

// BMP390 (temperature half) -- same chip as PressureSensor, logged as its
// own stream because it is its own measurement.
class TemperatureSensor : public SimulatedSensorBase {
public:
    explicit TemperatureSensor(unsigned seed = 2)
        : SimulatedSensorBase(SensorId::Temperature, /*baseline C*/ 22.0,
                               /*noise*/ 0.1,
                               config::kTemperatureMinC, config::kTemperatureMaxC, seed) {}
};

// ADXL375. The 200 g full scale is the reason this part was picked: it
// will not clip on ignition or parachute deployment shock. The simulated
// noise stays near ground-idle levels since there is no flight profile
// here -- only the bounds are flight-representative.
class VibrationSensor : public SimulatedSensorBase {
public:
    explicit VibrationSensor(unsigned seed = 3)
        : SimulatedSensorBase(SensorId::Vibration, /*baseline g*/ 0.0,
                               /*noise*/ 0.3,
                               config::kVibrationMinG, config::kVibrationMaxG, seed) {}
};

// SHT40. Slow-moving quantity, so a small noise term and 1 Hz sampling.
class HumiditySensor : public SimulatedSensorBase {
public:
    explicit HumiditySensor(unsigned seed = 4)
        : SimulatedSensorBase(SensorId::Humidity, /*baseline %RH*/ 45.0,
                               /*noise*/ 0.2,
                               config::kHumidityMinPct, config::kHumidityMaxPct, seed) {}
};

} // namespace payload
