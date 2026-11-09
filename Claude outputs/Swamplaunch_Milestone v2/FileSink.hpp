#pragma once
#include "LogSink.hpp"
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

namespace payload {
// Desktop only; the future Teensy SdSink implements the same interface.
class FileSink : public ILogSink {
public:
    explicit FileSink(std::filesystem::path directory) : directory_(std::move(directory)) {}
    ~FileSink() override { close(); }
    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;
    bool begin() override {
        std::error_code error;
        std::filesystem::create_directories(directory_, error);
        return !error && openNext();
    }
    bool openNext() override {
        close();
        while (next_ != 0) {
            char name[32]{};
            std::snprintf(name, sizeof(name), "LOG%04u.CSV", next_++);
            const auto path = directory_ / name;
            // Exclusive creation prevents overwrites, including concurrent creators.
            errno = 0;
            file_ = std::fopen(path.string().c_str(), "wbx");
            if (file_) { name_ = name; path_ = path; return true; }
            if (errno != EEXIST) return false;
        }
        return false;
    }
    std::size_t write(const std::uint8_t* data, std::size_t len) override {
        if (!file_) return 0;
        std::clearerr(file_);
        return std::fwrite(data, 1, len, file_);
    }
    bool flush() override { return file_ && std::fflush(file_) == 0; }
    void close() override { if (file_) { std::fclose(file_); file_ = nullptr; } }
    const char* fileName() const override { return name_.c_str(); }
    const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path directory_, path_;
    std::string name_;
    std::FILE* file_ = nullptr;
    unsigned next_ = 1;
};
}
