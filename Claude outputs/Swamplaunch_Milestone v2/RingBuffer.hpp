#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>

namespace payload {
// Fixed storage with a smaller logical capacity for configuration/tests.
// Single-threaded, like Logger; no ISR synchronization is provided.
template<class T, std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0, "A ring buffer needs storage");
public:
    explicit RingBuffer(std::size_t limit = Capacity) : limit_(limit) {
        if (limit == 0 || limit > Capacity) throw std::invalid_argument("Invalid queue size");
    }
    bool push(const T& value) {
        if (size_ == limit_) return false;
        data_[(head_ + size_) % limit_] = value;
        ++size_;
        if (size_ > highWater_) highWater_ = size_;
        return true;
    }
    bool pushOverwrite(const T& value) {
        const bool dropped = size_ == limit_;
        if (dropped) { head_ = (head_ + 1) % limit_; --size_; }
        push(value);
        return dropped;
    }
    bool pop(T& value) {
        if (empty()) return false;
        value = data_[head_];
        head_ = (head_ + 1) % limit_;
        --size_;
        return true;
    }
    const T* front() const { return empty() ? nullptr : &data_[head_]; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == limit_; }
    std::size_t size() const { return size_; }
    std::size_t highWater() const { return highWater_; }
private:
    std::array<T, Capacity> data_{};
    std::size_t limit_, head_ = 0, size_ = 0, highWater_ = 0;
};
}
