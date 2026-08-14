#include "tdx/market_cage.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void require_near(double actual, double expected, const std::string& message) {
    require(std::abs(actual - expected) < 1e-9,
            message + ": expected " + std::to_string(expected) +
            ", got " + std::to_string(actual));
}

}  // namespace

int main() {
    try {
        auto normal = tdx::calculate_tdx_market_cage(
            {10.00, 10.00, 9.99, 9.80, 11.00, 9.00, false, true, false});
        require_near(normal.upper, 10.20, "ordinary upper 2% cage");
        require_near(normal.lower, 9.80, "ordinary lower 2% cage");
        require_near(normal.upper_reference, 10.00, "upper uses best ask");
        require_near(normal.lower_reference, 10.00, "lower uses best bid");

        auto ten_ticks = tdx::calculate_tdx_market_cage(
            {2.00, 2.00, 1.99, 1.98, 3.00, 1.00, false, true, false});
        require_near(ten_ticks.upper, 2.10, "ten ticks dominate a small upper move");
        require_near(ten_ticks.lower, 1.90, "ten ticks dominate a small lower move");

        auto fallback = tdx::calculate_tdx_market_cage(
            {8.00, 0.00, 7.99, 7.90, 9.00, 7.00, false, true, false});
        require_near(fallback.upper_reference, 8.00, "missing ask falls back to bid");
        require_near(fallback.lower_reference, 8.00, "lower keeps the bid");

        auto beijing = tdx::calculate_tdx_market_cage(
            {10.00, 10.00, 10.00, 10.00, 20.00, 5.00, true, true, false});
        require_near(beijing.upper, 10.50, "Beijing upper 5% cage");
        require_near(beijing.lower, 9.50, "Beijing lower 5% cage");

        auto capped = tdx::calculate_tdx_market_cage(
            {10.00, 10.00, 10.00, 10.00, 10.15, 9.85, false, true, false});
        require_near(capped.upper, 10.15, "upper daily limit caps cage");
        require_near(capped.lower, 9.85, "lower daily limit caps cage");

        auto thousandths = tdx::calculate_tdx_market_cage(
            {1.000, 1.000, 1.000, 1.000, 2.000, 0.500, false, true, true});
        require_near(thousandths.upper, 1.020, "thousandth-tick upper cage");
        require_near(thousandths.lower, 0.980, "thousandth-tick lower cage");
        require_near(thousandths.tick_size, 0.001, "thousandth tick exposure");

        std::cout << "market cage tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
