#pragma once

#include <cstdint>
#include "IBus.hpp"
#include "Bmp390Math.hpp"
#include "payload/ISensor.hpp"

namespace bmp390 {

class Bmp390;

// One logged stream (PRESSURE or TEMPERATURE) backed by the shared chip.
// read() only hands back what the last Bmp390::update() fetched, so the two
// streams cost one bus transaction together and stay time aligned (BMP-7).
// PROVIDED: nothing to fill in here.
class Bmp390Stream : public payload::ISensor {
public:
    Bmp390Stream(const Bmp390& chip, payload::SensorId id) : chip_(chip), id_(id) {}
    payload::SensorId id() const override { return id_; }
    bool read(double& outValue, payload::StatusCode& outStatus) override;
    bool isOffline() const override;

private:
    const Bmp390& chip_;
    payload::SensorId id_;
};

class Bmp390 {
public:
    explicit Bmp390(IBus& bus);

    // Verify the chip, load calibration, configure normal mode at 100 Hz.
    // Returns false (never hangs) if the chip is missing or wrong (DRV-3).
    bool begin();

    // Call once per 10 ms from the main loop. Does ONE burst read of both
    // pressure and temperature and updates the cached results (BMP-7).
    // Must never wait on a conversion (DRV-4, BMP-4).
    void update();

    // The two ISensor views the logging loop polls.
    payload::ISensor& pressure() { return pressureStream_; }
    payload::ISensor& temperature() { return temperatureStream_; }

    bool isOffline() const { return offline_; }

    // Latest cached results, read by Bmp390Stream.
    double pressureKpa() const { return pressureKpa_; }
    double temperatureC() const { return temperatureC_; }
    payload::StatusCode pressureStatus() const { return pressureStatus_; }
    payload::StatusCode temperatureStatus() const { return temperatureStatus_; }

private:
    void registerFault();
    void registerSuccess();

    IBus& bus_;
    Calibration cal_{};

    double pressureKpa_ = 0.0;
    double temperatureC_ = 0.0;
    payload::StatusCode pressureStatus_ = payload::StatusCode::Timeout;
    payload::StatusCode temperatureStatus_ = payload::StatusCode::Timeout;

    uint32_t consecutiveFaults_ = 0;
    bool offline_ = false;

    Bmp390Stream pressureStream_;
    Bmp390Stream temperatureStream_;
};

} // namespace bmp390
