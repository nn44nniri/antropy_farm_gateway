#pragma once
#include "antropy_farm_gateway/types.hpp"

namespace antropy::gateway {

class Optimizer {
  public:
    explicit Optimizer(AlgorithmConfig config);
    OptimizationReportData run(const SensorSeriesMap& series_map, const std::string& metric_name) const;

  private:
    AlgorithmConfig config_;
};

} // namespace antropy::gateway
