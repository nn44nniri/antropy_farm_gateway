#pragma once
#include "antropy_farm_gateway/types.hpp"
#include <map>

namespace antropy::gateway {

Series average_series(const std::vector<Series>& series_group);
Series difference_series(const Series& a, const Series& b);
ErrorStats compute_error_stats(const Series& error, double tolerance);
double compute_rmse(const Series& reference, const Series& combination);
double compute_mape(const Series& reference, const Series& combination);
double shannon_entropy_from_binned_series(const Series& values, double bin_width);
double joint_entropy_approximation(const Series& a, const Series& b, double bin_width);
std::vector<std::vector<std::uint32_t>> generate_combinations(const std::vector<std::uint32_t>& sensor_ids,
                                                              std::size_t max_selected);

} // namespace antropy::gateway
