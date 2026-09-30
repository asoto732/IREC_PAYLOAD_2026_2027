#pragma once

#include <cstdint>

// Pure math for the BMP390: raw bytes in, engineering units out.
// Nothing in this file touches the bus, so all of it is testable on a
// laptop with `pio test -e native` (DRV-10). This is where most of
// tonight's work is.

namespace bmp390 {

// Calibration coefficients, already scaled to floating point.
// PROVIDED: the struct. YOUR JOB: fill it in parseCalibration().
struct Calibration {
    double parT1, parT2, parT3;
    double parP1, parP2, parP3, parP4, parP5, parP6;
    double parP7, parP8, parP9, parP10, parP11;
};

// Assemble a 24 bit unsigned value from three bytes sent least significant
// first (xlsb, lsb, msb). Used for both raw pressure and raw temperature.
uint32_t parse24(const uint8_t* bytes);

// Turn the 21 raw NVM bytes starting at register 0x31 into scaled
// coefficients (BMP-3).
Calibration parseCalibration(const uint8_t* raw21);

// Compensated temperature in degrees C from the raw 24 bit reading (BMP-3).
double compensateTemperature(uint32_t rawTemp, const Calibration& cal);

// Compensated pressure in PASCALS from the raw 24 bit reading and the
// already compensated temperature (BMP-3, BMP-10).
double compensatePressure(uint32_t rawPress, double tempC, const Calibration& cal);

} // namespace bmp390
