#include "antropy_farm_gateway/ring_buffer.hpp"
#include <algorithm>
#include <limits>

namespace antropy::gateway {

SensorHistory::SensorHistory(std::size_t window_size) : window_size_(window_size) {}

void SensorHistory::push(const SensorReading& reading) {
    auto& q = per_sensor_[reading.sensor_id];
    q.push_back(reading);
    while (q.size() > window_size_) q.pop_front();
}

std::optional<SensorReading> SensorHistory::latest(std::uint32_t sensor_id) const {
    auto it = per_sensor_.find(sensor_id);
    if (it == per_sensor_.end() || it->second.empty()) return std::nullopt;
    return it->second.back();
}

SensorSeriesMap SensorHistory::temperature_series() const {
    SensorSeriesMap out;
    const auto n = synchronized_sample_count();
    for (const auto& [id, q] : per_sensor_) {
        Series s; s.reserve(n);
        for (std::size_t i = q.size() - n; i < q.size(); ++i) s.push_back(q[i].temperature_c);
        out.emplace(id, std::move(s));
    }
    return out;
}

SensorSeriesMap SensorHistory::humidity_series() const {
    SensorSeriesMap out;
    const auto n = synchronized_sample_count();
    for (const auto& [id, q] : per_sensor_) {
        Series s; s.reserve(n);
        for (std::size_t i = q.size() - n; i < q.size(); ++i) s.push_back(q[i].humidity_pct);
        out.emplace(id, std::move(s));
    }
    return out;
}

std::size_t SensorHistory::synchronized_sample_count() const {
    if (per_sensor_.empty()) return 0;
    std::size_t n = std::numeric_limits<std::size_t>::max();
    for (const auto& [_, q] : per_sensor_) n = std::min(n, q.size());
    return n == std::numeric_limits<std::size_t>::max() ? 0 : n;
}

} // namespace antropy::gateway
