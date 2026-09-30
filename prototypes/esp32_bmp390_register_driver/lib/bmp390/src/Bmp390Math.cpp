#include "Bmp390Math.hpp"

namespace bmp390 {

uint32_t parse24(const uint8_t* bytes) {
    // TODO(BMP-3): bytes[0] is xlsb, bytes[1] is lsb, bytes[2] is msb.
    // Combine them into one 24 bit value with shifts and ORs.
    (void)bytes;
    return 0;
}

Calibration parseCalibration(const uint8_t* raw21) {
    Calibration cal{};

    // TODO(BMP-3): Decode each NVM coefficient, then scale it.
    //
    // Layout of the 21 bytes (index 0 = register 0x31). Multibyte values are
    // little endian (low byte first). Check this table against the
    // datasheet's "memory map" and "calibration coefficient" sections.
    //
    //   idx    name          type       scaled value
    //   0-1    NVM_PAR_T1    uint16     NVM * 2^8
    //   2-3    NVM_PAR_T2    uint16     NVM / 2^30
    //   4      NVM_PAR_T3    int8       NVM / 2^48
    //   5-6    NVM_PAR_P1    int16      (NVM - 2^14) / 2^20
    //   7-8    NVM_PAR_P2    int16      (NVM - 2^14) / 2^29
    //   9      NVM_PAR_P3    int8       NVM / 2^32
    //   10     NVM_PAR_P4    int8       NVM / 2^37
    //   11-12  NVM_PAR_P5    uint16     NVM * 2^3
    //   13-14  NVM_PAR_P6    uint16     NVM / 2^6
    //   15     NVM_PAR_P7    int8       NVM / 2^8
    //   16     NVM_PAR_P8    int8       NVM / 2^15
    //   17-18  NVM_PAR_P9    int16      NVM / 2^48
    //   19     NVM_PAR_P10   int8       NVM / 2^48
    //   20     NVM_PAR_P11   int8       NVM / 2^65
    //
    // Hints:
    //   * Watch signed vs unsigned. Casting a uint8_t to int8_t first is how
    //     you get a negative int8 coefficient.
    //   * std::ldexp(x, n) computes x * 2^n exactly and is cleaner than
    //     writing out powers of two.
    (void)raw21;
    return cal;
}

double compensateTemperature(uint32_t rawTemp, const Calibration& cal) {
    // TODO(BMP-3): Implement the datasheet's floating point temperature
    // compensation. It uses parT1, parT2 and parT3 only and is three lines.
    (void)rawTemp;
    (void)cal;
    return 0.0;
}

double compensatePressure(uint32_t rawPress, double tempC, const Calibration& cal) {
    // TODO(BMP-3, BMP-10): Implement the datasheet's floating point pressure
    // compensation. It uses parP1..parP11, the compensated temperature and
    // the raw pressure. Keep everything in double; do not round (BMP-10).
    // Result is in Pa. The driver converts to kPa, not this function.
    (void)rawPress;
    (void)tempC;
    (void)cal;
    return 0.0;
}

} // namespace bmp390
