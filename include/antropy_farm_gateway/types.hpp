#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace antropy::gateway {

struct SensorReading {
    std::uint32_t sensor_id{};
    double temperature_c{};
    double humidity_pct{};
    std::chrono::system_clock::time_point timestamp{};
};

struct SensorPosition {
    std::uint32_t sensor_id{};
    std::string label;
    double x_m{};
    double y_m{};
    double z_m{};
};

struct SurfaceConfig {
    std::string material;
    double thickness_m{};
    double u_value_w_m2k{};
};

struct VentConfig {
    std::string name;
    std::string wall;
    double x_m{};
    double y_m{};
    double z_m{};
    double width_m{};
    double height_m{};
    std::string kind;
};

struct FacilityConfig {
    std::string facility_id;
    double length_m{};
    double width_m{};
    double height_m{};
    double eave_height_m{};
    double ridge_height_m{};
    std::string orientation;
    SurfaceConfig ceiling;
    SurfaceConfig floor;
    SurfaceConfig north_wall;
    SurfaceConfig south_wall;
    SurfaceConfig east_wall;
    SurfaceConfig west_wall;
    std::vector<VentConfig> vents;
    std::vector<SensorPosition> sensor_positions;
};

struct Sx127xPinConfig {
    int spi_bus{0};
    int spi_chip_select{0};
    int gpio_mosi{10};
    int gpio_miso{9};
    int gpio_sck{11};
    int gpio_cs{8};
    int gpio_reset{25};
    int gpio_dio0{24};
    int gpio_dio1{23};
    int gpio_dio2{-1};
    int gpio_busy{-1};
    int gpio_rxen{-1};
    int gpio_txen{-1};
};

struct LoraConfig {
    std::string device;
    int baud_rate{};
    std::string run_mode{"real"};
    std::string mock_input_file;
    bool enable_mock_input{false};
    std::string transport{"spi"};
    std::string module_family{"sx127x"};
    Sx127xPinConfig sx127x_pins;
};

struct GrpcConfig {
    std::string uds_path;
    std::string interface_name;
    int deadline_ms{};
};

struct AlgorithmConfig {
    std::size_t max_selected_sensors{2};
    std::size_t min_history_per_sensor{12};
    double outlier_tolerance_temperature_c{0.1};
    double outlier_tolerance_humidity_pct{1.0};
    double entropy_bin_width_temperature_c{0.05};
    double entropy_bin_width_humidity_pct{0.5};
    std::size_t rolling_window_size{144};
};

struct DatabaseConfig {
    bool enabled{true};
    std::string path{"/var/lib/antropy_farm_gateway/antropy_gateway.db"};
};

struct Settings {
    FacilityConfig facility;
    LoraConfig lora;
    GrpcConfig grpc;
    AlgorithmConfig algorithm;
    DatabaseConfig database;
};

struct ErrorStats {
    double mean_error{};
    double stddev_error{};
    std::uint32_t outlier_count{};
    double z_index{};
};

struct CombinationScore {
    std::vector<std::uint32_t> sensors;
    double performance_index{};
    double total_entropy{};
    double rmse{};
    double mape{};
    ErrorStats error_stats;
};

struct OptimizationReportData {
    std::string metric_name;
    std::size_t total_sensors{};
    std::size_t samples_per_sensor{};
    std::vector<CombinationScore> error_based;
    std::vector<CombinationScore> entropy_based;
};

using Series = std::vector<double>;
using SensorSeriesMap = std::unordered_map<std::uint32_t, Series>;

} // namespace antropy::gateway
