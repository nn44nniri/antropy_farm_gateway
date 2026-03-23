#include "antropy_farm_gateway/grpc_client.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace antropy::gateway {
namespace {
std::string iso_utc_now() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
}

void append_f64(std::vector<std::uint8_t>& out, double v) {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(double));
    append_u64(out, bits);
}

void append_string(std::vector<std::uint8_t>& out, std::string_view s) {
    append_u32(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

bool write_all(int fd, const std::vector<std::uint8_t>& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto n = ::send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}
} // namespace

GrpcClient::GrpcClient(GrpcConfig config) : config_(std::move(config)) {}

std::vector<std::uint8_t> GrpcClient::encode_report(const FacilityConfig& facility,
                                                    const SensorHistory& history,
                                                    const OptimizationReportData& report) const {
    std::vector<std::uint8_t> out;
    out.reserve(4096);

    // Binary envelope for the sensors-interface UDS transport.
    append_string(out, "AFGW");            // magic
    append_u32(out, 1u);                    // protocol version
    append_string(out, config_.interface_name);
    append_string(out, facility.facility_id);
    append_string(out, iso_utc_now());
    append_string(out, report.metric_name);
    append_u32(out, static_cast<std::uint32_t>(report.total_sensors));
    append_u32(out, static_cast<std::uint32_t>(report.samples_per_sensor));

    append_u32(out, static_cast<std::uint32_t>(facility.sensor_positions.size()));
    for (const auto& pos : facility.sensor_positions) {
        append_u32(out, pos.sensor_id);
        append_string(out, pos.label);
        append_f64(out, pos.x_m);
        append_f64(out, pos.y_m);
        append_f64(out, pos.z_m);
        const auto latest = history.latest(pos.sensor_id);
        append_f64(out, latest ? latest->temperature_c : 0.0);
        append_f64(out, latest ? latest->humidity_pct : 0.0);
    }

    auto append_rows = [&](const std::vector<CombinationScore>& rows) {
        append_u32(out, static_cast<std::uint32_t>(rows.size()));
        for (const auto& row : rows) {
            append_u32(out, static_cast<std::uint32_t>(row.sensors.size()));
            for (auto id : row.sensors) append_u32(out, id);
            append_f64(out, row.performance_index);
            append_f64(out, row.total_entropy);
            append_f64(out, row.rmse);
            append_f64(out, row.mape);
            append_f64(out, row.error_stats.mean_error);
            append_f64(out, row.error_stats.stddev_error);
            append_u32(out, row.error_stats.outlier_count);
            append_f64(out, row.error_stats.z_index);
        }
    };

    append_rows(report.error_based);
    append_rows(report.entropy_based);
    return out;
}

bool GrpcClient::push_report(const FacilityConfig& facility,
                             const SensorHistory& history,
                             const OptimizationReportData& report) const {
    const auto payload = encode_report(facility, history, report);

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (config_.uds_path.size() >= sizeof(addr.sun_path)) {
        ::close(fd);
        return false;
    }
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", config_.uds_path.c_str());

    const bool connected = (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    if (!connected) {
        ::close(fd);
        return false;
    }

    const bool ok = write_all(fd, payload);
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
    return ok;
}

} // namespace antropy::gateway
