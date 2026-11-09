#include "FileSink.hpp"
#include "SyntheticFlight.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc > 3) throw std::invalid_argument("Usage: demo [output-directory] [flight-seconds:1..3600]");
        unsigned seconds = 10;
        if (argc == 3) {
            const std::string value = argv[2];
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("Flight seconds must be an integer in 1..3600");
            const auto parsed = std::stoul(value);
            if (parsed == 0 || parsed > 3600) throw std::invalid_argument("Flight seconds must be 1..3600");
            seconds = static_cast<unsigned>(parsed);
        }
        payload::FileSink sink(argc >= 2 ? argv[1] : "logs");
        // Queue storage is large: keep the desktop logger off the Windows stack.
        auto logger = std::make_unique<payload::Logger>(sink);
        if (!logger->begin()) throw std::runtime_error("Could not create a new numbered CSV file");
        payload::runSyntheticFlight(*logger, seconds);
        std::cout << "Wrote " << sink.path().string() << " (" << seconds
                  << " flight seconds + 1 pad second; 800 Hz ADXL375, 25 Hz BMP390; "
                  << logger->droppedRows() << " recording rows dropped)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
