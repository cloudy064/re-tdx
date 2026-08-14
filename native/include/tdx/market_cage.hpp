#pragma once

namespace tdx {

// Inputs used by TdxW's type-121 / selector-198 quote callback.  The callback
// chooses the best opposite-side quote first, then falls back through the
// other side, last price and previous close.
struct MarketCageInput {
    double best_bid{};
    double best_ask{};
    double last{};
    double previous_close{};
    double upper_limit{};
    double lower_limit{};
    bool beijing_market{};
    bool stock_security{true};
    bool thousandth_tick{};
};

struct MarketCagePrices {
    double upper{};
    double lower{};
    double upper_reference{};
    double lower_reference{};
    double tick_size{};
};

// Recovered DYNAINFO(28/29) core for TdxW's normal selector-198 path.  It is
// deliberately independent of networking so the exact rounding and fallback
// behavior can be tested and reused by formula and market-data surfaces.
MarketCagePrices calculate_tdx_market_cage(const MarketCageInput& input);

}  // namespace tdx
