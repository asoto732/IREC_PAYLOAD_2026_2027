#pragma once
#include "LogSink.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace payload {
// Desktop sink. Reuse it with a second Logger to simulate a reboot.
class MemorySink : public ILogSink {
public:
    bool begin() override { return !failBegin_ && openNext(); }
    bool openNext() override {
        close();
        for (;;) {
            char name[32]{};
            std::snprintf(name, sizeof(name), "LOG%04u.CSV", next_++);
            if (files_.count(name)) continue;
            name_ = name; files_.emplace(name_, ""); open_ = true; return true;
        }
    }
    std::size_t write(const std::uint8_t* data, std::size_t len) override {
        ++writeCalls;
        if (delayMs_) std::this_thread::sleep_for(std::chrono::milliseconds(delayMs_));
        if (!open_ || failWrites_) return 0;
        if (failNext_) { --failNext_; return 0; }
        const auto accepted = std::min(len, maxWriteBytes_);
        files_.at(name_).append(reinterpret_cast<const char*>(data), accepted);
        writeSizes.push_back(accepted);
        return accepted;
    }
    bool flush() override { ++flushCalls; return open_ && !failFlush_; }
    void close() override { if (open_) { open_ = false; ++closeCalls; } }
    const char* fileName() const override { return name_.c_str(); }
    const std::string& text() const { return files_.at(name_); }
    const std::map<std::string, std::string>& files() const { return files_; }
    void failBegin(bool value = true) { failBegin_ = value; }
    void failAllWrites(bool value = true) { failWrites_ = value; }
    void failNextWrites(unsigned count) { failNext_ = count; }
    void failFlushes(bool value = true) { failFlush_ = value; }
    void delayWrites(std::uint32_t ms) { delayMs_ = ms; }
    void limitWriteBytes(std::size_t bytes) { maxWriteBytes_ = bytes; }
    unsigned writeCalls = 0, flushCalls = 0, closeCalls = 0;
    std::vector<std::size_t> writeSizes;
private:
    std::map<std::string, std::string> files_;
    std::string name_;
    unsigned next_ = 1, failNext_ = 0;
    std::uint32_t delayMs_ = 0;
    std::size_t maxWriteBytes_ = static_cast<std::size_t>(-1);
    bool open_ = false, failBegin_ = false, failWrites_ = false, failFlush_ = false;
};
}
