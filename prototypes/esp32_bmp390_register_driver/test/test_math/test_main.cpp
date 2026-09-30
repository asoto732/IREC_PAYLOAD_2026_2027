// Host tests for the pure BMP390 math (DRV-10). Run: pio test -e native -f test_math
// All of these FAIL until the TODOs in Bmp390Math.cpp are filled in.

#include <unity.h>
#include <cmath>
#include "Bmp390Math.hpp"

using namespace bmp390;

void setUp() {}
void tearDown() {}

// Calibration bytes chosen so the scaled coefficients are easy numbers:
//   PAR_T1 = 100 * 2^8 = 25600, PAR_T2 = 16384 / 2^30 = 2^-16, PAR_T3 = -1 / 2^48
//   PAR_P1 = PAR_P2 = 0 (NVM 16384), PAR_P5 = 12665 * 8 = 101320, the rest 0
static const uint8_t kCalib[21] = {
    100, 0,        // T1
    0x00, 0x40,    // T2 = 16384
    0xFF,          // T3 = -1 (int8)
    0x00, 0x40,    // P1 = 16384
    0x00, 0x40,    // P2 = 16384
    0, 0,          // P3, P4
    0x79, 0x31,    // P5 = 12665
    0, 0,          // P6
    0, 0,          // P7, P8
    0, 0,          // P9
    0, 0           // P10, P11
};

void test_parse24_is_little_endian() {
    const uint8_t bytes[3] = {0x56, 0x34, 0x12};
    TEST_ASSERT_EQUAL_UINT32(0x123456u, parse24(bytes));
}

void test_calibration_scaling() {
    const Calibration cal = parseCalibration(kCalib);
    TEST_ASSERT_EQUAL_DOUBLE(25600.0, cal.parT1);
    TEST_ASSERT_EQUAL_DOUBLE(std::ldexp(1.0, -16), cal.parT2);
    TEST_ASSERT_EQUAL_DOUBLE(-std::ldexp(1.0, -48), cal.parT3);  // signed int8
    TEST_ASSERT_EQUAL_DOUBLE(0.0, cal.parP1);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, cal.parP2);
    TEST_ASSERT_EQUAL_DOUBLE(101320.0, cal.parP5);
}

void test_temperature_compensation() {
    Calibration cal{};
    cal.parT1 = 25600.0;
    cal.parT2 = std::ldexp(1.0, -16);
    cal.parT3 = 0.0;
    // 25 C worth of counts above PAR_T1
    const uint32_t raw = 25600u + 25u * 65536u;
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 25.0, compensateTemperature(raw, cal));
}

void test_pressure_offset_term() {
    Calibration cal{};
    cal.parP5 = 101320.0;  // with every other term zero, P == PAR_P5
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 101320.0, compensatePressure(0x6E0000u, 25.0, cal));
}

void test_pressure_linear_term() {
    Calibration cal{};
    cal.parP5 = 1000.0;
    cal.parP1 = 1.0;       // adds rawPress * PAR_P1
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1500.0, compensatePressure(500u, 25.0, cal));
}

// TODO(BMP-3): Once the board reads real data, record one raw pressure,
// raw temperature and the 21 calibration bytes, run them through Bosch's
// reference BMP3 Sensor API, and add that result as a test here.

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse24_is_little_endian);
    RUN_TEST(test_calibration_scaling);
    RUN_TEST(test_temperature_compensation);
    RUN_TEST(test_pressure_offset_term);
    RUN_TEST(test_pressure_linear_term);
    return UNITY_END();
}
