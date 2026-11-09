#pragma once
#include "CsvFormat.hpp"
#include "LogSink.hpp"
#include "RingBuffer.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

namespace payload {
struct LoggerConfig {
    // Section 5.2 placeholders, NOT measurements or flight-qualified settings.
    std::size_t pre_slow_rows = 256, pre_fast_rows = 512;
    std::size_t event_rows = 64, slow_rows = 256, fast_rows = 1024;
    std::uint32_t flush_interval_ms = 1000;
    unsigned max_write_retries = 3; // Additional attempts after initial failure.
};

class Logger {
public:
    enum class State { Uninitialized, Idle, Recording, Closing, Closed, Failed };
    static constexpr std::size_t kBlockBytes = 512, kStageBytes = 4096, kMaxRowBytes = 512;
    explicit Logger(ILogSink& sink, const LoggerConfig& cfg = {})
        : sink_(sink), config_(cfg), preSlow_(cfg.pre_slow_rows), preFast_(cfg.pre_fast_rows),
          events_(cfg.event_rows), slow_(cfg.slow_rows), fast_(cfg.fast_rows) {
        if (cfg.flush_interval_ms == 0) throw std::invalid_argument("Flush interval must be positive");
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool begin() {
        if (state_ != State::Uninitialized) return false;
        if (!sink_.begin()) { sink_.close(); state_ = State::Failed; return false; }
        sinkOpen_ = true;
        state_ = State::Idle;
        return true;
    }
    // Fixed-size validation and RAM copies only. No formatting, allocation or I/O.
    // true = accepted (sample queues may have evicted their oldest row).
    bool push(const LogRow& input) {
        if (state_ == State::Failed) { noteDrop(); return false; }
        if (state_ != State::Idle && state_ != State::Recording) return false;
        if (!validRow(input) || input.t_us > std::numeric_limits<std::uint64_t>::max() - timeOffset_)
            return false;
        StoredRow row = store(input);
        row.data.t_us += timeOffset_;
        if (row.data.t_us >= context_.t_us) context_ = row.data;
        if (state_ == State::Idle) {
            if (input.kind == RowKind::Adxl375) preFast_.pushOverwrite(row);
            else preSlow_.pushOverwrite(row);
            return true; // Intentional prehistory overwrites are not recording losses.
        }
        return enqueue(row);
    }
    bool startRecording(std::uint64_t now_us = 0, unsigned flight = 1,
                        unsigned reef = 0, std::uint32_t status = 0) {
        if (state_ != State::Idle || flight > 6 || reef > 3) return false;
        context_ = {};
        context_.t_us = now_us;
        context_.flight_state = flight; context_.reef_state = reef; context_.status = status;
        auto start = context_;
        // LOG_START timestamps the beginning of the retained history.
        if (preSlow_.front()) start.t_us = std::min(start.t_us, preSlow_.front()->data.t_us);
        if (preFast_.front()) start.t_us = std::min(start.t_us, preFast_.front()->data.t_us);
        prologue_[0] = makeEvent(start, "LOG_START:V1");
        prologueCount_ = 1;
        state_ = State::Recording;
        StoredRow row;
        while (preSlow_.pop(row)) enqueue(row);
        while (preFast_.pop(row)) enqueue(row);
        return true;
    }
    // After begin(), supply the saved state record. Subsequent push() times are
    // elapsed micros since THIS boot; the logger adds savedElapsedUs exactly once.
    bool recoverAfterReset(const char* previousFile, std::uint64_t savedElapsedUs,
                           unsigned flight = 1, unsigned reef = 0, std::uint32_t status = 0) {
        if (state_ != State::Idle || !preSlow_.empty() || !preFast_.empty() ||
            flight == 0 || flight >= 6 || reef > 3 || !previousFile) return false;
        char event[kEventBytes]{};
        constexpr char prefix[] = "RESET_RECOVERED:";
        std::size_t len = 0;
        while (len < kEventBytes - sizeof(prefix) && previousFile[len]) ++len;
        if (len == 0 || len == kEventBytes - sizeof(prefix)) return false;
        std::memcpy(event, prefix, sizeof(prefix) - 1);
        std::memcpy(event + sizeof(prefix) - 1, previousFile, len + 1);
        LogRow check; check.kind = RowKind::Event; check.event = event;
        if (!validRow(check) || !startRecording(savedElapsedUs, flight, reef, status)) return false;
        timeOffset_ = savedElapsedUs;
        prologue_[1] = makeEvent(context_, event);
        prologueCount_ = 2;
        return true;
    }
    // Asynchronous stop: keep calling service() until isClosed() or storageFailed().
    // Repeated calls, including after failure, are harmless.
    void close() {
        if (state_ == State::Idle) { closeSink(); state_ = State::Closed; }
        else if (state_ == State::Recording) state_ = State::Closing;
    }
    void service(std::uint32_t now_ms) {
        if (state_ != State::Recording && state_ != State::Closing) return;
        if (flushPending_) { tryFlush(now_ms); return; }
        if (pendingWrite_ != 0) { tryWrite(); return; }
        if (!headerStaged_) {
            append(std::string(kCsvHeader) + '\n'); headerStaged_ = true;
        }
        while (prologueIndex_ < prologueCount_) append(prologue_[prologueIndex_++]);
        queueDropReport();
        StoredRow row;
        while (stageLen_ + kMaxRowBytes <= kStageBytes) {
            if (!events_.pop(row) && !slow_.pop(row) && !fast_.pop(row)) break;
            append(row);
        }
        if (state_ == State::Closing && !stopStaged_ && queuesEmpty() && unreportedDrops_ == 0 &&
            stageLen_ + kMaxRowBytes <= kStageBytes) {
            append(makeEvent(context_, "LOG_STOP")); stopStaged_ = true;
        }
        const bool final = state_ == State::Closing && stopStaged_;
        const bool due = static_cast<std::uint32_t>(now_ms - lastFlushMs_) >= config_.flush_interval_ms;
        flushAfterWrite_ = due || final;
        finalFlush_ = final;
        pendingWrite_ = flushAfterWrite_ ? stageLen_ : (stageLen_ / kBlockBytes) * kBlockBytes;
        if (pendingWrite_) tryWrite();
        else if (flushAfterWrite_) { flushPending_ = true; tryFlush(now_ms); }
    }
    bool isRecording() const { return state_ == State::Recording; }
    bool isClosed() const { return state_ == State::Closed; }
    bool storageFailed() const { return state_ == State::Failed; }
    State state() const { return state_; }
    std::uint64_t droppedRows() const { return dropped_; }
    std::size_t fastHighWater() const { return fast_.highWater(); }
    const char* fileName() const { return sink_.fileName(); }
private:
    // data.event is unused: event text always travels with its row.
    struct StoredRow { LogRow data{}; std::array<char, kEventBytes> text{}; };
    static StoredRow store(const LogRow& input) {
        StoredRow row; row.data = input; row.data.event = nullptr;
        if (input.kind == RowKind::Event) std::memcpy(row.text.data(), input.event, std::strlen(input.event) + 1);
        return row;
    }
    static StoredRow makeEvent(LogRow context, const char* text) {
        context.kind = RowKind::Event; context.event = text;
        return store(context);
    }
    bool enqueue(const StoredRow& row) {
        if (row.data.kind == RowKind::Event) {
            if (!events_.push(row)) { noteDrop(); return false; }
        } else if (row.data.kind == RowKind::Adxl375) {
            if (fast_.pushOverwrite(row)) noteDrop();
        } else if (slow_.pushOverwrite(row)) noteDrop();
        return true;
    }
    void noteDrop() { ++dropped_; ++unreportedDrops_; }
    void queueDropReport() {
        if (!unreportedDrops_ || events_.full()) return;
        char text[kEventBytes]{};
        std::snprintf(text, sizeof(text), "ROWS_DROPPED:%llu", static_cast<unsigned long long>(unreportedDrops_));
        events_.push(makeEvent(context_, text));
        unreportedDrops_ = 0;
    }
    bool queuesEmpty() const { return events_.empty() && slow_.empty() && fast_.empty(); }
    void append(const StoredRow& row) {
        LogRow view = row.data;
        view.event = row.text.data();
        append(formatRow(view));
    }
    void append(const std::string& text) {
        if (text.empty() || text.size() > kMaxRowBytes || stageLen_ + text.size() > stage_.size())
            throw std::logic_error("CSV staging invariant failed");
        std::memcpy(stage_.data() + stageLen_, text.data(), text.size());
        stageLen_ += text.size();
    }
    void tryWrite() {
        const auto remaining = pendingWrite_ - writeOffset_;
        const auto accepted = sink_.write(stage_.data() + writeOffset_, remaining);
        if (accepted > remaining) { failStorage(); return; }
        writeOffset_ += accepted;
        if (writeOffset_ < pendingWrite_) { retryOrFail(); return; }
        std::memmove(stage_.data(), stage_.data() + pendingWrite_, stageLen_ - pendingWrite_);
        stageLen_ -= pendingWrite_;
        pendingWrite_ = 0; writeOffset_ = 0; retries_ = 0;
        if (flushAfterWrite_) flushPending_ = true;
    }
    void tryFlush(std::uint32_t now_ms) {
        if (!sink_.flush()) { retryOrFail(); return; }
        retries_ = 0; flushPending_ = false; lastFlushMs_ = now_ms;
        if (finalFlush_) { closeSink(); state_ = State::Closed; }
    }
    void retryOrFail() { if (++retries_ > config_.max_write_retries) failStorage(); }
    void failStorage() { state_ = State::Failed; closeSink(); }
    void closeSink() { if (sinkOpen_) { sink_.close(); sinkOpen_ = false; } }

    ILogSink& sink_;
    LoggerConfig config_;
    RingBuffer<StoredRow, 256> preSlow_;
    RingBuffer<StoredRow, 512> preFast_;
    RingBuffer<StoredRow, 64> events_;
    RingBuffer<StoredRow, 256> slow_;
    RingBuffer<StoredRow, 1024> fast_;
    std::array<std::uint8_t, kStageBytes> stage_{};
    std::array<StoredRow, 2> prologue_{};
    LogRow context_{};
    State state_ = State::Uninitialized;
    std::uint64_t timeOffset_ = 0, dropped_ = 0, unreportedDrops_ = 0;
    std::uint32_t lastFlushMs_ = 0;
    std::size_t stageLen_ = 0, pendingWrite_ = 0, writeOffset_ = 0;
    unsigned prologueCount_ = 0, prologueIndex_ = 0, retries_ = 0;
    bool sinkOpen_ = false, headerStaged_ = false, stopStaged_ = false;
    bool flushAfterWrite_ = false, flushPending_ = false, finalFlush_ = false;
};
}
