#pragma once

#include <cstddef>
#include <cstdint>

namespace bmp390 {

// The driver never touches Wire directly. It talks to this small interface
// instead, which buys us two things:
//   1. Host tests can hand the driver a FakeBus and run on a laptop with no
//      board attached (DRV-10).
//   2. Moving from the ESP32 to the Teensy 4.1 means a different IBus, not a
//      different driver (PLT-3).
//
// PROVIDED: nothing to fill in here.
class IBus {
public:
    virtual ~IBus() = default;

    // Read `len` bytes starting at register `reg` into `out`.
    // Returns false on any bus error or timeout (DRV-5).
    virtual bool readRegs(uint8_t reg, uint8_t* out, size_t len) = 0;

    // Write one byte to register `reg`. Returns false on any bus error.
    virtual bool writeReg(uint8_t reg, uint8_t value) = 0;
};

} // namespace bmp390
