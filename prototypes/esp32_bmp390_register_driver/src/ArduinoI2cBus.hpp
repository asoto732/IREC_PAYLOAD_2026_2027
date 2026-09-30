#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "IBus.hpp"

// IBus implemented on top of the Arduino TwoWire API. This is the only
// driver file that knows it is running on real hardware.
class ArduinoI2cBus : public bmp390::IBus {
public:
    ArduinoI2cBus(TwoWire& wire, uint8_t address) : wire_(wire), address_(address) {}

    // PROVIDED: use this as the worked example for readRegs().
    bool writeReg(uint8_t reg, uint8_t value) override {
        wire_.beginTransmission(address_);
        wire_.write(reg);
        wire_.write(value);
        return wire_.endTransmission() == 0;  // 0 means ACKed
    }

    bool readRegs(uint8_t reg, uint8_t* out, size_t len) override {
        // TODO(DRV-5): I2C register read in two steps.
        //   1. beginTransmission(address_), write(reg), then
        //      endTransmission(false). The `false` sends a repeated start
        //      instead of a stop. Nonzero return means failure.
        //   2. requestFrom(address_, static_cast<uint8_t>(len)). The cast
        //      matters: the ESP32 Wire library has several requestFrom
        //      overloads and an uncast size_t can be ambiguous. If it returns
        //      fewer than len bytes, fail. Otherwise copy each wire_.read()
        //      into out[].
        (void)reg;
        (void)out;
        (void)len;
        return false;
    }

private:
    TwoWire& wire_;
    uint8_t address_;
};
