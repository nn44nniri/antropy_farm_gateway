#pragma once
#include "antropy_farm_gateway/types.hpp"
#include "antropy_farm_gateway/ring_buffer.hpp"
#include <string>
#include <vector>

namespace antropy::gateway {

class GrpcClient {
  public:
    explicit GrpcClient(GrpcConfig config);
    bool push_report(const FacilityConfig& facility,
                     const SensorHistory& history,
                     const OptimizationReportData& report) const;

  private:
    std::vector<std::uint8_t> encode_report(const FacilityConfig& facility,
                                            const SensorHistory& history,
                                            const OptimizationReportData& report) const;

    GrpcConfig config_;
};

} // namespace antropy::gateway
