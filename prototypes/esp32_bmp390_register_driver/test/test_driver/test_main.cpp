// Host tests for the BMP390 driver logic using a fake bus (no hardware).
// Run: pio test -e native -f test_driver
// All of these FAIL until the TODOs in Bmp390.cpp and Bmp390Math.cpp are done.

#include <unity.h>
#include <cstring>
#include "Bmp390.hpp"
#include "Bmp390Regs.hpp"

using namespace bmp390;
using payload::StatusCode;

// A pretend BMP390: 256 registers in an array, plus a switch to make every
// bus call fail. Reads and writes go straight to the array.
class FakeBus : public IBus {
public:
    uint8_t regs[256] = {};
    bool fail = false;

    bool readRegs(uint8_t reg, uint8_t* out, size_t len) override {
        if (fail) return false;
        std::memcpy(out, &regs[reg], len);
        return true;
    }
    bool writeReg(uint8_t reg, uint8_t value) override {
        if (fail) return false;
        regs[reg] = value;
        return true;
    }
};

// Same easy calibration as test_math: T = 25 C for the raw temp below,
// P = PAR_P5 = 101320 Pa for any raw pressure.
static const uint8_t kCalib[21] = {100, 0, 0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x40,
                                   0, 0, 0x79, 0x31, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint32_t kRawTemp25C = 25600u + 25u * 65536u;

static void loadHealthyChip(FakeBus& bus) {
    std::memset(bus.regs, 0, sizeof(bus.regs));
    bus.regs[reg::kChipId] = reg::kChipIdValue;
    std::memcpy(&bus.regs[reg::kCalibStart], kCalib, sizeof(kCalib));
    bus.regs[reg::kStatus] = reg::kStatusDrdyPress | reg::kStatusDrdyTemp;
    // raw pressure (value does not matter with this calibration)
    bus.regs[reg::kData0 + 0] = 0x00;
    bus.regs[reg::kData0 + 1] = 0x00;
    bus.regs[reg::kData0 + 2] = 0x6E;
    // raw temperature, little endian
    bus.regs[reg::kData0 + 3] = kRawTemp25C & 0xFF;
    bus.regs[reg::kData0 + 4] = (kRawTemp25C >> 8) & 0xFF;
    bus.regs[reg::kData0 + 5] = (kRawTemp25C >> 16) & 0xFF;
}

void setUp() {}
void tearDown() {}

void test_begin_accepts_bmp390_and_enables_normal_mode() {        // BMP-2, BMP-4
    FakeBus bus;
    loadHealthyChip(bus);
    Bmp390 bmp(bus);
    TEST_ASSERT_TRUE(bmp.begin());
    TEST_ASSERT_EQUAL_HEX8(0x33, bus.regs[reg::kPwrCtrl]);
    TEST_ASSERT_EQUAL_HEX8(0x01, bus.regs[reg::kOdr]);
}

void test_begin_rejects_bmp388() {                                  // BMP-2
    FakeBus bus;
    loadHealthyChip(bus);
    bus.regs[reg::kChipId] = 0x50;
    Bmp390 bmp(bus);
    TEST_ASSERT_FALSE(bmp.begin());
}

void test_begin_fails_cleanly_with_no_chip() {                      // DRV-3
    FakeBus bus;
    bus.fail = true;
    Bmp390 bmp(bus);
    TEST_ASSERT_FALSE(bmp.begin());
}

void test_healthy_read_gives_both_streams() {                       // BMP-7, DRV-9
    FakeBus bus;
    loadHealthyChip(bus);
    Bmp390 bmp(bus);
    TEST_ASSERT_TRUE(bmp.begin());
    bmp.update();

    double v = 0; StatusCode s = StatusCode::CommError;
    TEST_ASSERT_TRUE(bmp.pressure().read(v, s));
    TEST_ASSERT_EQUAL(StatusCode::Ok, s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 101.320, v);   // kPa, not Pa

    TEST_ASSERT_TRUE(bmp.temperature().read(v, s));
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 25.0, v);
}

void test_no_new_data_is_timeout() {                                // BMP-8
    FakeBus bus;
    loadHealthyChip(bus);
    Bmp390 bmp(bus);
    TEST_ASSERT_TRUE(bmp.begin());
    bus.regs[reg::kStatus] = 0;  // data ready bits clear
    bmp.update();

    double v = 0; StatusCode s = StatusCode::Ok;
    TEST_ASSERT_FALSE(bmp.pressure().read(v, s));
    TEST_ASSERT_EQUAL(StatusCode::Timeout, s);
}

void test_out_of_range_keeps_value() {                              // DRV-6
    FakeBus bus;
    loadHealthyChip(bus);
    bus.regs[reg::kCalibStart + 11] = 0xE8;  // PAR_P5 NVM = 1000 -> 8000 Pa = 8 kPa
    bus.regs[reg::kCalibStart + 12] = 0x03;
    Bmp390 bmp(bus);
    TEST_ASSERT_TRUE(bmp.begin());
    bmp.update();

    double v = 0; StatusCode s = StatusCode::Ok;
    TEST_ASSERT_FALSE(bmp.pressure().read(v, s));
    TEST_ASSERT_EQUAL(StatusCode::OutOfRange, s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 8.0, v);
}

void test_repeated_bus_faults_take_both_streams_offline() {         // DRV-7, BMP-9
    FakeBus bus;
    loadHealthyChip(bus);
    Bmp390 bmp(bus);
    TEST_ASSERT_TRUE(bmp.begin());
    bus.fail = true;

    double v = 0; StatusCode s = StatusCode::Ok;
    for (int i = 0; i < 4; ++i) bmp.update();
    TEST_ASSERT_FALSE(bmp.isOffline());
    bmp.pressure().read(v, s);
    TEST_ASSERT_EQUAL(StatusCode::CommError, s);

    bmp.update();  // fifth fault in a row
    TEST_ASSERT_TRUE(bmp.pressure().isOffline());
    TEST_ASSERT_TRUE(bmp.temperature().isOffline());
    bmp.temperature().read(v, s);
    TEST_ASSERT_EQUAL(StatusCode::SensorOffline, s);
}

void test_one_good_read_resets_fault_count() {                      // DRV-7
    FakeBus bus;
    loadHealthyChip(bus);
    Bmp390 bmp(bus);
    TEST_ASSERT_TRUE(bmp.begin());

    bus.fail = true;
    for (int i = 0; i < 4; ++i) bmp.update();
    bus.fail = false;
    bmp.update();                       // good read resets the count
    bus.fail = true;
    for (int i = 0; i < 4; ++i) bmp.update();
    TEST_ASSERT_FALSE(bmp.isOffline());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_accepts_bmp390_and_enables_normal_mode);
    RUN_TEST(test_begin_rejects_bmp388);
    RUN_TEST(test_begin_fails_cleanly_with_no_chip);
    RUN_TEST(test_healthy_read_gives_both_streams);
    RUN_TEST(test_no_new_data_is_timeout);
    RUN_TEST(test_out_of_range_keeps_value);
    RUN_TEST(test_repeated_bus_faults_take_both_streams_offline);
    RUN_TEST(test_one_good_read_resets_fault_count);
    return UNITY_END();
}
