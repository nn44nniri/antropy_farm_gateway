#include "antropy_farm_gateway/optimizer.hpp"
#include "antropy_farm_gateway/metrics.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace antropy::gateway {
namespace {
struct Intermediate {
    CombinationScore score;
};

std::vector<Series> lookup_series(const SensorSeriesMap& map, const std::vector<std::uint32_t>& ids) {
    std::vector<Series> group;
    group.reserve(ids.size());
    for (auto id : ids) group.push_back(map.at(id));
    return group;
}
}

Optimizer::Optimizer(AlgorithmConfig config) : config_(std::move(config)) {}

OptimizationReportData Optimizer::run(const SensorSeriesMap& series_map, const std::string& metric_name) const {
    OptimizationReportData out;
    out.metric_name = metric_name;
    out.total_sensors = series_map.size();
    if (series_map.empty()) return out;
    out.samples_per_sensor = series_map.begin()->second.size();

    std::vector<std::uint32_t> ids;
    ids.reserve(series_map.size());
    for (const auto& [id, _] : series_map) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    const Series reference = average_series(lookup_series(series_map, ids));
    const auto combos = generate_combinations(ids, config_.max_selected_sensors);
    std::vector<Intermediate> rows;
    rows.reserve(combos.size());

    const double tolerance = metric_name == "temperature_c"
        ? config_.outlier_tolerance_temperature_c
        : config_.outlier_tolerance_humidity_pct;
    const double bin_width = metric_name == "temperature_c"
        ? config_.entropy_bin_width_temperature_c
        : config_.entropy_bin_width_humidity_pct;

    // Article reference-trend formalism: R_t = (1/n) * sum_{k=1..n} T_{k,t}.
    // Variables: R_t = reference trend at time t, T_{k,t} = measurement at sensor k and time t, n = total sensors.
    // Summary: all installed sensors are averaged to define the target whole-facility trend.
    //
    // Article combination-trend formalism: C_t = (1/p) * sum_{k in S} T_{k,t}.
    // Variables: C_t = combination trend, S = selected sensor subset, p = number of selected sensors.
    // Summary: each candidate subset is reduced to one averaged trend and compared against the reference.
    //
    // Article error formalism: E_t = R_t - C_t.
    // Summary: the closer E_t stays to zero, the more representative the subset is.
    //
    // Article Eq. (5): RMSE = sqrt(sum((R_i - C_i)^2)/N).
    // Article Eq. (6): MAPE = 100/N * sum(|(R_i - C_i)/R_i|).
    // Article Fig. 5: z-index = mean(Error) / sd(Error).
    for (const auto& combo : combos) {
        const Series comb = average_series(lookup_series(series_map, combo));
        const Series err = difference_series(reference, comb);
        Intermediate row;
        row.score.sensors = combo;
        row.score.error_stats = compute_error_stats(err, tolerance);
        row.score.rmse = compute_rmse(reference, comb);
        row.score.mape = compute_mape(reference, comb);

        // Article Eq. (2): H(X) = -sum_i p_i log2(p_i).
        // Variables: H(X) = entropy of one selected sensor, p_i = probability mass in bin i.
        // Summary: selected sensors with higher entropy retain more climate variability information.
        double entropy_sum = 0.0;
        for (auto id : combo) entropy_sum += shannon_entropy_from_binned_series(series_map.at(id), bin_width);

        // Article Eq. (4) total-entropy-style combination used by the package:
        //   T_combo ~= sum_{k in S} H(X_k) + sum_{i notin S} sum_{j in S} H(X_i, X_j)
        // Variables: S = selected sensors, H(X_k) = single-sensor entropy, H(X_i, X_j) = joint entropy term.
        // Summary: reward subsets that are informative themselves and also capture information from omitted locations.
        double delivered = 0.0;
        for (auto id_a : ids) {
            if (std::find(combo.begin(), combo.end(), id_a) != combo.end()) continue;
            for (auto id_b : combo) delivered += joint_entropy_approximation(series_map.at(id_a), series_map.at(id_b), bin_width);
        }
        row.score.total_entropy = entropy_sum + delivered;
        rows.push_back(std::move(row));
    }

    auto rank_and_score = [&](auto accessor, auto smaller_better) {
        std::vector<std::size_t> order(rows.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const double va = accessor(rows[a].score);
            const double vb = accessor(rows[b].score);
            return smaller_better ? (va < vb) : (va > vb);
        });
        std::vector<double> scores(rows.size(), 0.0);
        const double total_non_empty = std::pow(2.0, static_cast<double>(ids.size())) - 1.0;
        for (std::size_t rank = 0; rank < order.size(); ++rank) {
            // Article Table 1 scoring rule: S_i = (2^n - 1 - R + 1).
            // Variables: n = total installed sensors, R = rank of a combination for one statistic.
            // Summary: better ranks receive larger scores before the multi-criterion sum.
            scores[order[rank]] = total_non_empty - static_cast<double>(rank + 1) + 1.0;
        }
        return scores;
    };

    const auto s1 = rank_and_score([](const CombinationScore& s){ return std::abs(s.error_stats.mean_error); }, true);
    const auto s2 = rank_and_score([](const CombinationScore& s){ return s.error_stats.stddev_error; }, true);
    const auto s3 = rank_and_score([](const CombinationScore& s){ return static_cast<double>(s.error_stats.outlier_count); }, true);
    const auto s4 = rank_and_score([](const CombinationScore& s){ return std::abs(s.error_stats.z_index); }, true);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        // Article Eq. (1): I_i = S1(E_i) + S2(E_i) + S3(E_i) + S4(E_i).
        // Variables: I_i = performance index of combination i; S1..S4 are the rank-derived scores from
        // mean error, standard deviation, outlier count, and z-index respectively.
        // Summary: the subset with the largest I_i is the best representative placement in the error method.
        rows[i].score.performance_index = s1[i] + s2[i] + s3[i] + s4[i];
    }

    std::sort(rows.begin(), rows.end(), [](const Intermediate& a, const Intermediate& b) {
        return a.score.performance_index > b.score.performance_index;
    });
    for (std::size_t i = 0; i < std::min<std::size_t>(5, rows.size()); ++i) out.error_based.push_back(rows[i].score);

    std::sort(rows.begin(), rows.end(), [](const Intermediate& a, const Intermediate& b) {
        return a.score.total_entropy > b.score.total_entropy;
    });
    for (std::size_t i = 0; i < std::min<std::size_t>(5, rows.size()); ++i) out.entropy_based.push_back(rows[i].score);

    return out;
}

} // namespace antropy::gateway
