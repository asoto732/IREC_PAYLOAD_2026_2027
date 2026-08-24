#pragma once

#include <random>
#include "payload/ISensor.hpp"
#include "payload/Config.hpp"

namespace payload {

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

// Baselines/bounds are placeholders representative of a small sounding
// payload; replace with real sensor datasheet ranges once selected (SEN-7).
class PressureSensor : public SimulatedSensorBase {
public:
    explicit PressureSensor(unsigned seed = 1)
        : SimulatedSensorBase(SensorId::Pressure, /*baseline kPa*/ 101.3,
                               /*noise*/ 0.05, /*lower*/ 0.0, /*upper*/ 120.0, seed) {}
};

class TemperatureSensor : public SimulatedSensorBase {
public:
    explicit TemperatureSensor(unsigned seed = 2)
        : SimulatedSensorBase(SensorId::Temperature, /*baseline C*/ 22.0,
                               /*noise*/ 0.1, /*lower*/ -40.0, /*upper*/ 85.0, seed) {}
};

class VibrationSensor : public SimulatedSensorBase {
public:
    explicit VibrationSensor(unsigned seed = 3)
        : SimulatedSensorBase(SensorId::Vibration, /*baseline g*/ 0.0,
                               /*noise*/ 0.3, /*lower*/ -16.0, /*upper*/ 16.0, seed) {}
};

} // namespace payload
