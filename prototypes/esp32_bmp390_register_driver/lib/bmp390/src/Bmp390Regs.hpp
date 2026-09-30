#pragma once

#include <cstdint>

// BMP390 register map. PROVIDED, but CHECK EVERY VALUE against the Bosch
// BMP390 datasheet before trusting it. Finding a wrong number here on paper
// is much cheaper than finding it on the bench.

namespace bmp390::reg {

constexpr uint8_t kChipId     = 0x00;  // reads 0x60 on a BMP390 (0x50 = BMP388)
constexpr uint8_t kErr        = 0x02;
constexpr uint8_t kStatus     = 0x03;  // bit5 drdy_press, bit6 drdy_temp
constexpr uint8_t kData0      = 0x04;  // 6 bytes: press xlsb,lsb,msb then temp xlsb,lsb,msb
constexpr uint8_t kPwrCtrl    = 0x1B;  // bit0 press_en, bit1 temp_en, bits5:4 mode (0b11 = normal)
constexpr uint8_t kOsr        = 0x1C;  // bits2:0 osr_p, bits5:3 osr_t
constexpr uint8_t kOdr        = 0x1D;  // odr_sel: 0x00 = 200 Hz, 0x01 = 100 Hz, 0x02 = 50 Hz
constexpr uint8_t kConfig     = 0x1F;  // bits3:1 IIR filter coefficient
constexpr uint8_t kCalibStart = 0x31;  // 21 bytes of NVM_PAR_T1 .. NVM_PAR_P11
constexpr uint8_t kCmd        = 0x7E;

constexpr uint8_t kChipIdValue   = 0x60;
constexpr uint8_t kSoftResetCmd  = 0xB6;
constexpr uint8_t kStatusDrdyPress = 1u << 5;
constexpr uint8_t kStatusDrdyTemp  = 1u << 6;

constexpr uint8_t kCalibLength = 21;
constexpr uint8_t kDataLength  = 6;

} // namespace bmp390::reg
