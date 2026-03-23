#include "antropy_farm_gateway/lora_receiver.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace antropy::gateway {

LoraReceiver::LoraReceiver(LoraConfig config) : config_(std::move(config)) {}

SensorReading LoraReceiver::parse_line(const std::string& line) const {
    // Expected SX127x gateway payload format (CSV over serial or mock file):
    // sensor_id,temperature_c,humidity_pct,unix_ms
    std::stringstream ss(line);
    std::string tok;
    SensorReading r;
    std::getline(ss, tok, ','); r.sensor_id = static_cast<std::uint32_t>(std::stoul(tok));
    std::getline(ss, tok, ','); r.temperature_c = std::stod(tok);
    std::getline(ss, tok, ','); r.humidity_pct = std::stod(tok);
    if (std::getline(ss, tok, ',')) {
        const auto ms = std::stoll(tok);
        r.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds(ms)};
    } else {
        r.timestamp = std::chrono::system_clock::now();
    }
    return r;
}

void LoraReceiver::receive_loop(const std::function<void(const SensorReading&)>& on_reading) {
    if (config_.enable_mock_input) {
        std::ifstream in(config_.mock_input_file);
        if (!in) throw std::runtime_error("Failed to open mock input file");
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            on_reading(parse_line(line));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return;
    }

    std::ifstream in(config_.device);
    if (!in) {
        throw std::runtime_error(
            "Failed to open configured sensor device path for real execution: " + config_.device +
            ". In test mode the gateway reads config/mock_lora_input.csv, but in real mode it only reads live sensor lines from the configured device path.");
    }

    std::string line;
    while (true) {
        if (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            on_reading(parse_line(line));
            continue;
        }
        if (in.eof()) {
            in.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        throw std::runtime_error("Failed while reading live sensor input from device path: " + config_.device);
    }
}

} // namespace antropy::gateway
