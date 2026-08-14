#include "tdx/market_cage.hpp"

#include <algorithm>
#include <cmath>

namespace tdx {
namespace {

constexpr double present_threshold = 0.0007;
constexpr double equality_threshold = 0.00009;

bool present(double value) {
    return std::isfinite(value) && value >= present_threshold;
}

double first_present(double first, double second, double third, double fourth) {
    if (present(first)) return first;
    if (present(second)) return second;
    if (present(third)) return third;
    return std::isfinite(fourth) ? fourth : 0.0;
}

double truncated(double value) {
    return static_cast<double>(static_cast<long long>(value));
}

}  // namespace

MarketCagePrices calculate_tdx_market_cage(const MarketCageInput& input) {
    const double scale = input.thousandth_tick ? 1000.0 : 100.0;
    const double tick = 1.0 / scale;
    const double ten_ticks = 10.0 / scale;
    const double upper_reference = first_present(
        input.best_ask, input.best_bid, input.last, input.previous_close);
    const double lower_reference = first_present(
        input.best_bid, input.best_ask, input.last, input.previous_close);

    double upper = 0.0;
    if (input.beijing_market) {
        // TdxW uses truncation with a tiny positive bias for the 5% branch.
        upper = truncated(1.05 * upper_reference * scale + 0.003) / scale;
    } else {
        const double movement = truncated(
            0.02 * upper_reference * scale + 0.5 + 0.001) / scale;
        upper = truncated((upper_reference + movement) * scale + 0.5 + 0.001) /
                scale;
    }
    if (upper > equality_threshold &&
        std::abs(upper - upper_reference) < equality_threshold)
        upper += tick;
    if (input.stock_security)
        upper = std::max(upper, upper_reference + ten_ticks);
    if (present(input.upper_limit) && input.upper_limit <= upper)
        upper = input.upper_limit;

    double lower = input.beijing_market
        ? truncated(0.95 * lower_reference * scale + 0.997) / scale
        : truncated(0.98 * lower_reference * scale + 0.5 + 0.001) / scale;
    if (lower > equality_threshold &&
        std::abs(lower - lower_reference) < equality_threshold)
        lower = std::max(tick, lower - tick);
    if (input.stock_security)
        lower = std::min(lower, std::max(tick, lower_reference - ten_ticks));
    if (present(input.lower_limit) && input.lower_limit >= lower)
        lower = input.lower_limit;

    return MarketCagePrices{upper, lower, upper_reference, lower_reference, tick};
}

}  // namespace tdx
