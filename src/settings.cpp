#include "antropy_farm_gateway/settings.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace antropy::gateway {
namespace {
using boost::property_tree::ptree;

ptree load_ptree(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to open settings file: " + path);
    ptree tree;
    boost::property_tree::read_json(in, tree);
    return tree;
}

const ptree& require_child(const ptree& parent, const std::string& key) {
    auto opt = parent.get_child_optional(key);
    if (!opt) throw std::runtime_error("Missing JSON object key: " + key);
    return opt.get();
}

std::string get_string(const ptree& obj, const std::string& key, const std::string& def = "") {
    return obj.get<std::string>(key, def);
}

double get_double(const ptree& obj, const std::string& key, double def = 0.0) {
    return obj.get<double>(key, def);
}

int get_int(const ptree& obj, const std::string& key, int def = 0) {
    return obj.get<int>(key, def);
}

bool get_bool(const ptree& obj, const std::string& key, bool def = false) {
    return obj.get<bool>(key, def);
}

SurfaceConfig parse_surface(const ptree& j) {
    return SurfaceConfig{get_string(j, "material"), get_double(j, "thickness_m"), get_double(j, "u_value_w_m2k")};
}

Sx127xPinConfig parse_sx127x_pins(const ptree& j) {
    Sx127xPinConfig c;
    c.spi_bus = get_int(j, "spi_bus", 0);
    c.spi_chip_select = get_int(j, "spi_chip_select", 0);
    c.gpio_mosi = get_int(j, "gpio_mosi", 10);
    c.gpio_miso = get_int(j, "gpio_miso", 9);
    c.gpio_sck = get_int(j, "gpio_sck", 11);
    c.gpio_cs = get_int(j, "gpio_cs", 8);
    c.gpio_reset = get_int(j, "gpio_reset", 25);
    c.gpio_dio0 = get_int(j, "gpio_dio0", 24);
    c.gpio_dio1 = get_int(j, "gpio_dio1", 23);
    c.gpio_dio2 = get_int(j, "gpio_dio2", -1);
    c.gpio_busy = get_int(j, "gpio_busy", -1);
    c.gpio_rxen = get_int(j, "gpio_rxen", -1);
    c.gpio_txen = get_int(j, "gpio_txen", -1);
    return c;
}
} // namespace

Settings load_settings(const std::string& path) {
    const auto root = load_ptree(path);

    Settings s;
    const auto& f = require_child(root, "facility");
    s.facility.facility_id = get_string(f, "facility_id", "facility-1");
    s.facility.length_m = get_double(f, "length_m");
    s.facility.width_m = get_double(f, "width_m");
    s.facility.height_m = get_double(f, "height_m");
    s.facility.eave_height_m = get_double(f, "eave_height_m");
    s.facility.ridge_height_m = get_double(f, "ridge_height_m");
    s.facility.orientation = get_string(f, "orientation", "north-south");
    s.facility.ceiling = parse_surface(require_child(f, "ceiling"));
    s.facility.floor = parse_surface(require_child(f, "floor"));
    s.facility.north_wall = parse_surface(require_child(f, "north_wall"));
    s.facility.south_wall = parse_surface(require_child(f, "south_wall"));
    s.facility.east_wall = parse_surface(require_child(f, "east_wall"));
    s.facility.west_wall = parse_surface(require_child(f, "west_wall"));

    const auto& vents = require_child(f, "vents");
    for (const auto& item : vents) {
        const auto& v = item.second;
        s.facility.vents.push_back(VentConfig{
            get_string(v, "name"), get_string(v, "wall"), get_double(v, "x_m"), get_double(v, "y_m"),
            get_double(v, "z_m"), get_double(v, "width_m"), get_double(v, "height_m"), get_string(v, "kind")
        });
    }

    const auto& sensors = require_child(f, "sensor_positions");
    for (const auto& item : sensors) {
        const auto& p = item.second;
        s.facility.sensor_positions.push_back(SensorPosition{
            static_cast<std::uint32_t>(get_int(p, "sensor_id", 0)), get_string(p, "label"),
            get_double(p, "x_m"), get_double(p, "y_m"), get_double(p, "z_m")
        });
    }

    const auto& l = require_child(root, "lora");
    s.lora.device = get_string(l, "device", "/dev/spidev0.0");
    s.lora.baud_rate = get_int(l, "baud_rate", 115200);
    s.lora.run_mode = get_string(l, "run_mode", "real");
    s.lora.mock_input_file = get_string(l, "mock_input_file");
    s.lora.enable_mock_input = (s.lora.run_mode == "test") && get_bool(l, "enable_mock_input", true);
    s.lora.transport = get_string(l, "transport", "spi");
    s.lora.module_family = get_string(l, "module_family", "sx127x");
    if (auto pins = l.get_child_optional("sx127x_pins")) {
        s.lora.sx127x_pins = parse_sx127x_pins(pins.get());
    }

    const auto& g = require_child(root, "grpc");
    s.grpc.uds_path = get_string(g, "uds_path", "/tmp/sensors-interface.sock");
    s.grpc.interface_name = get_string(g, "interface_name", "sensors-interface");
    s.grpc.deadline_ms = get_int(g, "deadline_ms", 3000);


    const auto& d = require_child(root, "database");
    s.database.enabled = get_bool(d, "enabled", true);
    s.database.path = get_string(d, "path", "data/antropy_gateway.db");

    const auto& a = require_child(root, "algorithm");
    s.algorithm.max_selected_sensors = static_cast<std::size_t>(get_int(a, "max_selected_sensors", 2));
    s.algorithm.min_history_per_sensor = static_cast<std::size_t>(get_int(a, "min_history_per_sensor", 12));
    s.algorithm.outlier_tolerance_temperature_c = get_double(a, "outlier_tolerance_temperature_c", 0.1);
    s.algorithm.outlier_tolerance_humidity_pct = get_double(a, "outlier_tolerance_humidity_pct", 1.0);
    s.algorithm.entropy_bin_width_temperature_c = get_double(a, "entropy_bin_width_temperature_c", 0.05);
    s.algorithm.entropy_bin_width_humidity_pct = get_double(a, "entropy_bin_width_humidity_pct", 0.5);
    s.algorithm.rolling_window_size = static_cast<std::size_t>(get_int(a, "rolling_window_size", 144));

    return s;
}

} // namespace antropy::gateway
