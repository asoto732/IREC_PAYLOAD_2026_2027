#include "Bmp390.hpp"
#include "Bmp390Config.hpp"
#include "Bmp390Regs.hpp"

using payload::SensorId;
using payload::StatusCode;

namespace bmp390 {

// ---------------------------------------------------------------- streams
// PROVIDED.

bool Bmp390Stream::read(double& outValue, StatusCode& outStatus) {
    if (chip_.isOffline()) {
        outStatus = StatusCode::SensorOffline;
        return false;
    }
    const bool isPressure = (id_ == SensorId::Pressure);
    outValue  = isPressure ? chip_.pressureKpa() : chip_.temperatureC();
    outStatus = isPressure ? chip_.pressureStatus() : chip_.temperatureStatus();
    return outStatus == StatusCode::Ok;
}

bool Bmp390Stream::isOffline() const { return chip_.isOffline(); }

// ------------------------------------------------------------------ chip

Bmp390::Bmp390(IBus& bus)
    : bus_(bus),
      pressureStream_(*this, SensorId::Pressure),
      temperatureStream_(*this, SensorId::Temperature) {}

bool Bmp390::begin() {
    // TODO(BMP-2): Read reg::kChipId. Return false if the read fails or the
    // value is not reg::kChipIdValue. A BMP388 (0x50) must be rejected.

    // TODO(BMP-3): Read reg::kCalibLength bytes starting at reg::kCalibStart
    // and store parseCalibration(...) in cal_. Return false if the read fails.

    // TODO(BMP-4, BMP-5, BMP-6): Write the configuration registers using the
    // constants in Bmp390Config.hpp: kOsr, kOdr, kConfig, and LAST kPwrCtrl
    // (turning on normal mode last means the chip starts converting with
    // the right settings). Return false if any write fails.

    return false;  // TODO: return true once everything above succeeded
}

void Bmp390::update() {
    if (offline_) {
        return;
    }

    // TODO(BMP-8): Read reg::kStatus. If the bus read fails, set BOTH
    // statuses to CommError, call registerFault(), and return.
    // If either data ready bit (kStatusDrdyPress, kStatusDrdyTemp) is clear,
    // no new conversion has finished: set both statuses to Timeout, call
    // registerFault(), and return. Never log the same value twice.

    // TODO(BMP-7): ONE burst read of reg::kDataLength bytes from reg::kData0.
    // On failure: both statuses CommError, registerFault(), return.

    // TODO(BMP-3, BMP-10): Use parse24() on bytes 0..2 (pressure) and 3..5
    // (temperature), then compensateTemperature() and compensatePressure().
    // Store temperature in C and pressure in kPa (the math returns Pa).

    // TODO(DRV-6, SEN-10): Range check each value against Bmp390Config.hpp.
    // Out of range: status OutOfRange, KEEP the value so it is visible.
    // If either stream faulted call registerFault(), else registerSuccess().
}

// PROVIDED: fault counting shared by both streams (DRV-7, BMP-9).
void Bmp390::registerFault() {
    ++consecutiveFaults_;
    if (consecutiveFaults_ >= config::kOfflineAfterConsecutiveFaults) {
        offline_ = true;
    }
}

void Bmp390::registerSuccess() { consecutiveFaults_ = 0; }

} // namespace bmp390
