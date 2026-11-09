#pragma once
#include <cstddef>
#include <cstdint>

namespace payload {
class ILogSink {
public:
    // Open a new unique file. Logger supplies the header and version event.
    virtual bool begin() = 0;
    virtual bool openNext() = 0;
    // Bytes accepted, including partial writes. Zero means failure.
    // Logger retries only the unaccepted suffix, preventing duplicate CSV bytes.
    virtual std::size_t write(const std::uint8_t* data, std::size_t len) = 0;
    virtual bool flush() = 0;
    virtual void close() = 0;
    virtual const char* fileName() const = 0;
    virtual ~ILogSink() = default;
};
}
