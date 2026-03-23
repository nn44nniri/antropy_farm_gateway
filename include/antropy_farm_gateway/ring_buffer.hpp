#pragma once
#include "antropy_farm_gateway/types.hpp"
#include <optional>
#include <unordered_map>

namespace antropy::gateway {

class SensorHistory {
  public:
    explicit SensorHistory(std::size_t window_size);
    void push(const SensorReading& reading);
    std::optional<SensorReading> latest(std::uint32_t sensor_id) const;
    SensorSeriesMap temperature_series() const;
    SensorSeriesMap humidity_series() const;
    std::size_t synchronized_sample_count() const;

  private:
    std::size_t window_size_;
    std::unordered_map<std::uint32_t, std::deque<SensorReading>> per_sensor_;
};

} // namespace antropy::gateway
