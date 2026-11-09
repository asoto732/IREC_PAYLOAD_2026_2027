#include "MemorySink.hpp"
#include "FileSink.hpp"
#include "Logger.hpp"
#include "SyntheticFlight.hpp"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace payload;
using CsvRow = std::array<std::string, 24>;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::vector<CsvRow> parse(const std::string& text) {
    std::vector<CsvRow> result;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        CsvRow row;
        std::size_t start = 0;
        for (std::size_t i = 0; i < row.size(); ++i) {
            const auto comma = line.find(',', start);
            check((i == 23) == (comma == std::string::npos), "CSV must have exactly 24 columns");
            row[i] = line.substr(start, comma == std::string::npos ? comma : comma - start);
            start = comma == std::string::npos ? line.size() : comma + 1;
        }
        result.push_back(row);
    }
    return result;
}
std::unique_ptr<Logger> started(MemorySink& sink, const LoggerConfig& cfg = {}) {
    auto logger = std::make_unique<Logger>(sink, cfg);
    check(logger->begin(), "begin failed");
    check(logger->startRecording(), "start failed");
    return logger;
}
void finish(Logger& logger, std::uint32_t now = 1000) {
    logger.close();
    for (unsigned i = 0; i < 10000 && !logger.isClosed() && !logger.storageFailed(); ++i)
        logger.service(now);
    check(logger.isClosed(), "clean close did not complete");
}
LogRow sample(std::uint64_t t, RowKind kind = RowKind::Adxl375) {
    LogRow row; row.t_us = t; row.kind = kind; row.flight_state = 1;
    row.values = kind == RowKind::Bmp390 ? std::array<double, 6>{12, 101325, 22.5} :
                                         std::array<double, 6>{0.1, -0.2, 1};
    return row;
}
LogRow event(std::uint64_t t, const char* text) {
    auto row = sample(t, RowKind::Event); row.event = text; return row;
}
std::vector<CsvRow> kindRows(const std::vector<CsvRow>& csv, const std::string& kind) {
    std::vector<CsvRow> rows;
    for (const auto& row : csv) if (row[1] == kind) rows.push_back(row);
    return rows;
}
void lt1() {
    MemorySink sink;
    auto logger = started(sink);
    check(!logger->begin(), "second begin must fail without rewriting header");
    check(!logger->startRecording(), "second start must not duplicate version event");
    check(logger->push(sample(10, RowKind::Bmp390)), "BMP push");
    check(logger->push(sample(20)), "ADXL push");
    auto act = sample(30, RowKind::Act); act.values = {2, 5, -123, 1.25, 12.1, 24.2};
    check(logger->push(act), "ACT without encoder");
    act.t_us = 40; act.has_motor_position = true;
    check(logger->push(act), "ACT with encoder");
    auto ctrl = sample(50, RowKind::Ctrl); ctrl.values = {5, 6, 7, -1, 20, 19};
    check(logger->push(ctrl), "CTRL push");
    finish(*logger);
    const auto csv = parse(sink.text());
    check(csv.size() == 8, "header + five measurements + start/stop");
    check(sink.text().find(kCsvHeader) == 0 && sink.text().find(kCsvHeader, 1) == std::string::npos,
          "header must be written once");
    for (std::size_t n = 1; n < csv.size(); ++n) {
        const auto& r = csv[n];
        check(!r[0].empty() && !r[1].empty() && r[4] == "0x0", "common fields");
        for (std::size_t i = 5; i < 24; ++i) {
            bool filled = (r[1] == "BMP390" && i >= 5 && i <= 7) ||
                (r[1] == "ADXL375" && i >= 8 && i <= 10) ||
                (r[1] == "ACT" && i >= 11 && i <= 16 && (i != 13 || r[0] == "40")) ||
                (r[1] == "CTRL" && i >= 17 && i <= 22) || (r[1] == "EVENT" && i == 23);
            check(r[i].empty() != filled, "irrelevant fields must remain blank");
        }
    }
    const auto acts = kindRows(csv, "ACT");
    check(acts[1][13] == "-123" && acts[1][12] == "5", "ACT integer fields");
    check(kindRows(csv, "BMP390")[0][6] == "101325", "pressure must stay in Pa");
    act.values[2] = -2147483648.0;
    check(parse(formatRow(act))[0][13] == "-2147483648", "encoder count precision at int32 minimum");
    act.values[2] = 2147483647.0;
    check(parse(formatRow(act))[0][13] == "2147483647", "encoder count precision at int32 maximum");
}
void lt2() {
    MemorySink sink;
    auto logger = std::make_unique<Logger>(sink);
    check(logger->begin(), "begin");
    runSyntheticFlight(*logger, 1);
    const auto csv = parse(sink.text());
    const auto fast = kindRows(csv, "ADXL375"), slow = kindRows(csv, "BMP390");
    check(fast.size() == 1312 && slow.size() == 50, "exact synthetic rate/history counts");
    for (const auto& kind : {"ADXL375", "BMP390", "EVENT"}) {
        const auto rows = kindRows(csv, kind);
        for (std::size_t i = 1; i < rows.size(); ++i)
            check(std::stoull(rows[i][0]) >= std::stoull(rows[i - 1][0]), "timestamps monotonic per kind");
    }
    for (std::size_t i = 0; i < fast.size(); ++i)
        check(std::stoull(fast[i][0]) == 360000 + i * 1250, "800 Hz timestamp spacing");
    for (std::size_t i = 0; i < slow.size(); ++i)
        check(std::stoull(slow[i][0]) == i * 40000, "25 Hz timestamp spacing");
}
void lt3() {
    MemorySink sink;
    LoggerConfig cfg; cfg.pre_fast_rows = 3; cfg.pre_slow_rows = 3;
    auto logger = std::make_unique<Logger>(sink, cfg);
    check(logger->begin(), "begin");
    for (unsigned t = 0; t < 5; ++t) {
        auto fast = sample(t), slow = sample(t, RowKind::Bmp390);
        fast.flight_state = slow.flight_state = 0;
        check(logger->push(fast) && logger->push(slow), "idle samples");
    }
    char text[] = "FLAG:PAD";
    auto e = event(5, text); e.flight_state = 0;
    check(logger->push(e), "prelaunch event");
    text[5] = 'X';
    logger->service(1000);
    check(sink.writeCalls == 0 && sink.text().empty(), "IDLE must not write rows");
    check(logger->startRecording(10), "BOOST");
    finish(*logger);
    const auto csv = parse(sink.text());
    const auto fast = kindRows(csv, "ADXL375"), slow = kindRows(csv, "BMP390");
    check(fast.size() == 3 && fast[0][0] == "2", "pre fast wrap keeps newest");
    check(slow.size() == 2 && slow[0][0] == "3", "shared pre slow/event wrap");
    check(sink.text().find("FLAG:PAD") != std::string::npos, "event text must be copied");
    check(logger->droppedRows() == 0, "prehistory trimming is intentional");
}
void lt4() {
    MemorySink sink;
    LoggerConfig cfg; cfg.fast_rows = 3; cfg.slow_rows = 2;
    auto logger = started(sink, cfg);
    for (unsigned i = 0; i < 10; ++i) check(logger->push(sample(i)), "fast overwrite accepts row");
    for (unsigned i = 0; i < 4; ++i) check(logger->push(sample(i, RowKind::Bmp390)), "slow push");
    check(logger->push(event(10, "STATE:COAST")), "event one");
    check(logger->push(event(11, "ACT_CMD:MOTOR:PARTIAL")), "event two");
    check(logger->droppedRows() == 9 && sink.writeCalls == 0, "drop counter and no push I/O");
    finish(*logger);
    const auto csv = parse(sink.text());
    const auto fast = kindRows(csv, "ADXL375"), slow = kindRows(csv, "BMP390");
    check(fast.size() == 3 && fast[0][0] == "7" && fast[2][0] == "9", "oldest fast rows dropped");
    check(slow.size() == 2 && slow[0][0] == "2", "oldest slow rows dropped");
    check(csv[2][23] == "STATE:COAST" && csv[3][23] == "ACT_CMD:MOTOR:PARTIAL" &&
          csv[4][23] == "ROWS_DROPPED:9" && csv[5][1] == "BMP390", "event/slow/fast priority");
}
void lt5() {
    MemorySink sink;
    auto logger = started(sink);
    sink.failAllWrites();
    check(logger->push(sample(1)), "push must not touch failing sink");
    logger->close();
    for (unsigned i = 0; i < 10; ++i) logger->service(1000);
    check(logger->storageFailed() && sink.writeCalls == 4, "bounded retries then FAILED");
    const auto drops = logger->droppedRows();
    check(!logger->push(sample(2)) && logger->droppedRows() == drops + 1, "failed pushes counted");
    logger->close(); logger->close();
    check(sink.closeCalls == 1, "failed sink closed once");
    MemorySink missing; missing.failBegin();
    auto absent = std::make_unique<Logger>(missing);
    check(!absent->begin() && absent->storageFailed(), "missing storage reported");
}
void lt6() {
    MemorySink sink;
    auto logger = started(sink);
    check(logger->push(sample(1)), "push");
    logger->close(); logger->close();
    check(!logger->isRecording(), "close must stop accepting samples immediately");
    check(!logger->push(sample(2)), "late sample rejected");
    finish(*logger);
    const auto text = sink.text();
    logger->close(); logger->service(10000);
    check(sink.text() == text && sink.closeCalls == 1 && sink.flushCalls == 1, "close is idempotent");
    const auto csv = parse(text);
    check(csv.back()[23] == "LOG_STOP", "last row must be LOG_STOP");
    MemorySink idle;
    auto idleLogger = std::make_unique<Logger>(idle);
    check(idleLogger->begin(), "idle begin");
    idleLogger->close(); idleLogger->close();
    check(idleLogger->isClosed() && idle.closeCalls == 1, "close idle safely");
}
void lt7() {
    const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::path("build") / ("sink-test-" + std::to_string(id));
    std::filesystem::create_directories(directory);
    { std::ofstream old(directory / "LOG0001.CSV"); old << "keep me"; }
    FileSink first(directory);
    check(first.begin() && std::string(first.fileName()) == "LOG0002.CSV", "skip existing file");
    const std::string contents = "new file\n";
    check(first.write(reinterpret_cast<const std::uint8_t*>(contents.data()), contents.size()) == contents.size(), "disk write");
    check(first.flush() && first.openNext() && std::string(first.fileName()) == "LOG0003.CSV", "openNext numbering");
    first.close(); first.close();
    FileSink second(directory);
    check(second.begin() && std::string(second.fileName()) == "LOG0004.CSV", "fresh sink scans existing files");
    second.close();
    std::ifstream old(directory / "LOG0001.CSV");
    std::string kept; std::getline(old, kept);
    check(kept == "keep me", "existing file unchanged");
    std::ifstream written(directory / "LOG0002.CSV");
    std::getline(written, kept); check(kept == "new file", "file sink bytes persisted");
}
void lt8() {
    MemorySink sink;
    auto before = started(sink);
    check(before->push(sample(10000)), "old sample");
    finish(*before);
    const auto previous = sink.text();
    auto after = std::make_unique<Logger>(sink);
    check(after->begin() && std::string(after->fileName()) == "LOG0002.CSV", "recovery opens new file");
    constexpr std::uint64_t saved = 5000000000ULL;
    check(!after->recoverAfterReset("bad,file", saved), "invalid recovery metadata rejected");
    check(after->recoverAfterReset("LOG0001.CSV", saved, 3, 0, 0x80), "recover reset");
    auto row = sample(1250); row.flight_state = 3; row.status = 0x80;
    check(after->push(row), "recovered sample");
    finish(*after);
    const auto csv = parse(sink.text());
    check(csv[1][23] == "LOG_START:V1" && csv[2][23] == "RESET_RECOVERED:LOG0001.CSV", "recovery prologue first");
    check(csv[1][0] == "5000000000" && csv[2][4] == "0x80", "saved time/status restored");
    check(kindRows(csv, "ADXL375")[0][0] == "5000001250", "64-bit offset added once");
    check(sink.files().at("LOG0001.CSV") == previous, "previous session preserved");
}
void lt12() {
    MemorySink sink;
    sink.delayWrites(2); // Artificial latency; no measured card stall is available yet.
    auto logger = started(sink);
    // Model 100 ms without sink service while an independent acquisition schedule
    // supplies RAM rows. This is queue-capacity evidence, not a timing proof.
    for (std::uint64_t t = 0; t < 1000000; t += 1250) {
        const auto calls = sink.writeCalls;
        check(logger->push(sample(t)), "stall fast sample");
        if (t % 40000 == 0) check(logger->push(sample(t, RowKind::Bmp390)), "stall slow sample");
        check(calls == sink.writeCalls, "push must never call sleeping sink");
        if (t < 100000 || t >= 200000) logger->service(static_cast<std::uint32_t>(t / 1000));
    }
    finish(*logger);
    const auto csv = parse(sink.text());
    check(logger->fastHighWater() >= 80 && logger->droppedRows() == 0, "100 ms queued without drops");
    check(kindRows(csv, "ADXL375").size() == 800 && kindRows(csv, "BMP390").size() == 25,
          "stall loses no measurements");
}
void lt13() {
    MemorySink sink;
    auto logger = started(sink);
    for (unsigned bit = 0; bit < 32; ++bit) {
        const auto status = std::uint32_t{1} << bit;
        auto row = sample(bit, RowKind::Bmp390); row.status = status;
        check(logger->push(row), "fault status sample");
        const auto code = "FAULT:TEST" + std::to_string(bit);
        row.kind = RowKind::Event; row.event = code.c_str();
        check(logger->push(row), "fault event");
    }
    finish(*logger);
    const auto csv = parse(sink.text());
    const auto slow = kindRows(csv, "BMP390"), events = kindRows(csv, "EVENT");
    check(slow.size() == 32 && events.size() == 34, "all injected faults retained");
    for (unsigned bit = 0; bit < 32; ++bit) {
        check(std::stoul(slow[bit][4], nullptr, 16) == (std::uint32_t{1} << bit), "status bit preserved");
        check(events[bit + 1][23] == "FAULT:TEST" + std::to_string(bit) &&
              events[bit + 1][4] == slow[bit][4], "fault event and status correspond");
    }
}
void lt14() {
    MemorySink sink;
    auto logger = std::make_unique<Logger>(sink);
    check(logger->begin(), "begin");
    runSyntheticFlight(*logger, 1);
    check(logger->isClosed() && logger->droppedRows() == 0, "flight stops cleanly");
    const auto csv = parse(sink.text());
    bool boost = false, landed = false;
    for (const auto& row : csv) {
        if (row[23] == "STATE:BOOST") { check(row[0] == "1000000" && row[2] == "1", "BOOST time/state"); boost = true; }
        if (row[23] == "STATE:LANDED") { check(row[0] == "2000000" && row[2] == "6", "LANDED time/state"); landed = true; }
    }
    check(boost && landed && csv.back()[23] == "LOG_STOP" && csv.back()[2] == "6", "flight lifecycle");
}
void retryAndShortWrites() {
    MemorySink expectedSink, actualSink;
    auto expected = started(expectedSink), actual = started(actualSink);
    actualSink.failNextWrites(2);
    actualSink.limitWriteBytes(300); // Force a partial write through a CSV row.
    for (unsigned i = 0; i < 4; ++i) {
        check(expected->push(sample(i)) && actual->push(sample(i)), "retry sample");
    }
    finish(*expected); finish(*actual);
    check(expectedSink.text() == actualSink.text(), "partial retries must not duplicate or lose bytes");
    check(actualSink.writeCalls > expectedSink.writeCalls, "fault injection exercised retries");
}
void eventOverflow() {
    MemorySink sink;
    LoggerConfig cfg; cfg.event_rows = 2;
    auto logger = started(sink, cfg);
    check(logger->push(event(1, "FLAG:ONE")) && logger->push(event(2, "FLAG:TWO")), "fill events");
    check(!logger->push(event(3, "FLAG:THREE")), "full events reject newest");
    finish(*logger);
    check(logger->droppedRows() == 1 && sink.text().find("ROWS_DROPPED:1") != std::string::npos,
          "event drop eventually reported when slot becomes free");
    check(sink.text().find("FLAG:ONE") != std::string::npos && sink.text().find("FLAG:TWO") != std::string::npos &&
          sink.text().find("FLAG:THREE") == std::string::npos, "never overwrite a queued event");
}
void flushAndBlocks() {
    MemorySink sink;
    auto logger = started(sink);
    logger->service(0); // Header + start remain below one block.
    check(sink.writeCalls == 0, "sub-block tail waits");
    logger->service(999);
    check(sink.writeCalls == 0, "fixed flush interval");
    logger->service(1000); logger->service(1000);
    check(sink.writeCalls == 1 && sink.flushCalls == 1 && !sink.text().empty(), "flush commits a short tail");
    for (unsigned i = 0; i < 50; ++i) check(logger->push(sample(i)), "block sample");
    const auto before = sink.writeCalls;
    logger->service(1001);
    check(sink.writeCalls == before + 1 && sink.writeSizes.back() % 512 == 0, "one whole-block write per pass");
    finish(*logger, 1002);
    MemorySink bad;
    auto failed = started(bad); bad.failFlushes(); failed->close();
    for (unsigned i = 0; i < 10; ++i) failed->service(1000);
    check(failed->storageFailed() && bad.flushCalls == 4, "flush faults use bounded retries");
}
void malformedAndRing() {
    MemorySink sink;
    auto logger = std::make_unique<Logger>(sink);
    check(!logger->push(sample(1)), "push before begin");
    check(logger->begin() && logger->startRecording(), "start");
    for (const char* text : {"bad,event", "bad\nevent", "bad\revent", "bad\"event", ""})
        check(!logger->push(event(1, text)), "malformed event rejected");
    check(!logger->push(event(1, nullptr)), "null event rejected");
    std::string longEvent(kEventBytes, 'x');
    check(!logger->push(event(1, longEvent.c_str())), "oversize event rejected");
    auto invalid = sample(1); invalid.values[0] = std::numeric_limits<double>::quiet_NaN();
    check(!logger->push(invalid), "NaN rejected");
    invalid = sample(1); invalid.kind = static_cast<RowKind>(255);
    check(!logger->push(invalid), "invalid row kind rejected");
    finish(*logger);
    check(parse(sink.text()).size() == 3, "invalid input never corrupts file");
    RingBuffer<int, 3> ring;
    check(ring.push(1) && ring.push(2) && ring.push(3) && !ring.push(4), "ring full");
    check(ring.pushOverwrite(4), "ring overwrite reported");
    int n = 0;
    check(ring.pop(n) && n == 2 && ring.pop(n) && n == 3 && ring.pop(n) && n == 4 && !ring.pop(n), "ring FIFO wrap");
}
}

int main() {
    const std::pair<const char*, void(*)()> tests[] = {
        {"LT1 header and five row layouts", lt1}, {"LT2 timestamps and 800/25 Hz streams", lt2},
        {"LT3 retained PAD history and copied events", lt3}, {"LT4 overflow and priority", lt4},
        {"LT5 storage failures and bounded retries", lt5}, {"LT6 idempotent clean close", lt6},
        {"LT7 exclusive numbered files", lt7}, {"LT8 simulated reset and saved time", lt8},
        {"LT12 artificial storage stall", lt12}, {"LT13 caller-supplied fault bits/events", lt13},
        {"LT14 BOOST through LANDED", lt14}, {"retry and partial-write integrity", retryAndShortWrites},
        {"event queue overflow", eventOverflow}, {"flush interval and block writes", flushAndBlocks},
        {"malformed input and ring wrap", malformedAndRing}
    };
    unsigned failures = 0;
    for (const auto& test : tests) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << test.first << ": " << error.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
