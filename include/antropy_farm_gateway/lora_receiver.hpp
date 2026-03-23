#pragma once
#include "antropy_farm_gateway/types.hpp"
#include <functional>
#include <string>

namespace antropy::gateway {

class LoraReceiver {
  public:
    explicit LoraReceiver(LoraConfig config);
    void receive_loop(const std::function<void(const SensorReading&)>& on_reading);

  private:
    LoraConfig config_;
    SensorReading parse_line(const std::string& line) const;
};

} // namespace antropy::gateway
