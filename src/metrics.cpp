#include "antropy_farm_gateway/metrics.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <stdexcept>

namespace antropy::gateway {
namespace {
std::size_t count_set_bits(std::size_t value) {
    std::size_t count = 0;
    while (value != 0) {
        count += (value & static_cast<std::size_t>(1));
        value >>= 1;
    }
    return count;
}

double mean(const Series& s) {
    if (s.empty()) return 0.0;
    return std::accumulate(s.begin(), s.end(), 0.0) / static_cast<double>(s.size());
}

double stddev(const Series& s, double m) {
    if (s.size() < 2) return 0.0;
    double acc = 0.0;
    for (double v : s) acc += (v - m) * (v - m);
    return std::sqrt(acc / static_cast<double>(s.size()));
}

std::map<int, double> pmf(const Series& values, double bin_width) {
    std::map<int, double> probs;
    if (values.empty()) return probs;
    for (double v : values) {
        const int bin = static_cast<int>(std::floor(v / bin_width));
        probs[bin] += 1.0;
    }
    for (auto& [_, p] : probs) p /= static_cast<double>(values.size());
    return probs;
}
}

// Article averaging used for both the reference trend and each combination trend:
//   R_t = (1/n) * sum_{k=1..n} T_{k,t}
//   C_t = (1/p) * sum_{k in S} T_{k,t}
// where T_{k,t} is the value measured by sensor k at time t, n is the total number of sensors,
// p is the number of selected sensors, R_t is the all-sensor reference trend, and C_t is the
// selected-sensor combination trend. Summary: this is the plain arithmetic mean used by the article.
Series average_series(const std::vector<Series>& group) {
    if (group.empty()) return {};
    const auto n = group.front().size();
    Series out(n, 0.0);
    for (const auto& s : group) {
        if (s.size() != n) throw std::runtime_error("Series length mismatch");
        for (std::size_t i = 0; i < n; ++i) out[i] += s[i];
    }
    for (double& v : out) v /= static_cast<double>(group.size());
    return out;
}

// Article error trend used for ranking combinations:
//   E_t = R_t - C_t
// where R_t is the reference trend, C_t is the combination trend, and E_t is the time-indexed error.
// Summary: this measures how far a chosen sensor subset deviates from the all-sensor view.
Series difference_series(const Series& a, const Series& b) {
    if (a.size() != b.size()) throw std::runtime_error("Series length mismatch");
    Series out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = a[i] - b[i];
    return out;
}

// Article statistics computed from the error trend include mean(E), sd(E), outlier count, and
// z-index = mean(E) / sd(E). Here tolerance is the engineering threshold used to count outliers.
// Summary: these four statistics feed the article-style error-based ranking.
ErrorStats compute_error_stats(const Series& error, double tolerance) {
    const double m = mean(error);
    const double sd = stddev(error, m);
    std::uint32_t outliers = 0;
    for (double e : error) if (std::abs(e) > tolerance) ++outliers;
    return ErrorStats{m, sd, outliers, sd > 0.0 ? m / sd : 0.0};
}

// Article Eq. (5):
//   RMSE = sqrt( (1/N) * sum_{i=1..N} (R_i - C_i)^2 )
// where R_i is the reference value, C_i is the combination value, and N is the number of samples.
// Summary: RMSE verifies the absolute deviation of a selected subset from the full reference trend.
double compute_rmse(const Series& reference, const Series& combination) {
    const Series diff = difference_series(reference, combination);
    double acc = 0.0;
    for (double d : diff) acc += d * d;
    return diff.empty() ? 0.0 : std::sqrt(acc / static_cast<double>(diff.size()));
}

// Article Eq. (6):
//   MAPE = (100/N) * sum_{i=1..N} |(R_i - C_i) / R_i|
// where R_i is the reference value, C_i is the combination value, and N is the number of valid samples.
// Summary: MAPE verifies the percentage deviation of a selected subset from the full reference trend.
double compute_mape(const Series& reference, const Series& combination) {
    if (reference.size() != combination.size()) throw std::runtime_error("Series length mismatch");
    double acc = 0.0;
    std::size_t used = 0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
        if (std::abs(reference[i]) < 1e-9) continue;
        acc += std::abs((reference[i] - combination[i]) / reference[i]);
        ++used;
    }
    return used == 0 ? 0.0 : 100.0 * acc / static_cast<double>(used);
}

// Article Eq. (2) uses Shannon entropy on binned measurements:
//   H(X) = - sum_i p_i * log2(p_i)
// where p_i is the probability mass of bin i and X is one sensor series after discretization.
// Summary: higher entropy indicates a location with more variability and information content.
double shannon_entropy_from_binned_series(const Series& values, double bin_width) {
    const auto probs = pmf(values, bin_width);
    double h = 0.0;
    for (const auto& [_, p] : probs) if (p > 0.0) h -= p * std::log2(p);
    return h;
}

// Article Eq. (3) relates joint entropy as H(X,Y) = H(X) + H(Y|X).
// This implementation estimates joint entropy from paired bins of two synchronized sensor series.
// Variables: X and Y are the two sensor series, and p(x,y) is their joint binned probability mass.
// Summary: joint entropy approximates the information shared and delivered across sensor locations.
double joint_entropy_approximation(const Series& a, const Series& b, double bin_width) {
    if (a.size() != b.size()) throw std::runtime_error("Series length mismatch");
    std::map<std::pair<int,int>, double> joint;
    for (std::size_t i = 0; i < a.size(); ++i) {
        joint[{static_cast<int>(std::floor(a[i] / bin_width)), static_cast<int>(std::floor(b[i] / bin_width))}] += 1.0;
    }
    double h = 0.0;
    for (auto& [_, p] : joint) {
        p /= static_cast<double>(a.size());
        if (p > 0.0) h -= p * std::log2(p);
    }
    return h;
}

std::vector<std::vector<std::uint32_t>> generate_combinations(const std::vector<std::uint32_t>& sensor_ids,
                                                              std::size_t max_selected) {
    std::vector<std::vector<std::uint32_t>> out;
    const std::size_t n = sensor_ids.size();
    const std::size_t total = static_cast<std::size_t>(1) << n;
    for (std::size_t mask = 1; mask < total; ++mask) {
        if (count_set_bits(mask) > max_selected) continue;
        std::vector<std::uint32_t> combo;
        for (std::size_t i = 0; i < n; ++i) if (mask & (static_cast<std::size_t>(1) << i)) combo.push_back(sensor_ids[i]);
        out.push_back(std::move(combo));
    }
    return out;
}

} // namespace antropy::gateway
