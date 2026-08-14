#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

struct OptionInstrument {
    int market_id{};
    std::string wire_code;
    std::string name;
    std::string contract;
    int underlying_market_id{};
    std::string underlying_code;
    bool call{};
    double strike{};
    bool american{};
    bool futures_model{};
};

struct OptionAnalytics {
    double price{};
    double delta{};
    double gamma{};
    double theta{};
    double vega{};
    double rho{};
};

std::optional<OptionInstrument> parse_option_instrument(
    int market_id, std::string wire_code, std::string name);
Json option_instrument_document(const OptionInstrument& option);
Json resolve_option_expiry_document(const std::filesystem::path& root,
                                    const OptionInstrument& option);
Json normalize_option_catalog_document(
    const Json& instrument_directory, int market_filter = -1,
    const std::string& underlying = {}, const std::string& contract = {},
    const std::string& type = "all", const std::string& query = {},
    int limit = 20000);
Json fetch_option_catalog_document(
    int market_filter = -1, const std::string& underlying = {},
    const std::string& contract = {}, const std::string& type = "all",
    const std::string& query = {}, int limit = 20000,
    bool refresh = false, int cache_ttl_seconds = 300,
    int timeout_ms = 30000);

double tdx_normal_cdf(double value);
double tdx_historical_volatility(const std::vector<double>& closes);
double tdx_option_price(bool futures_model, bool american, bool call,
                        double divisor, double underlying, double strike,
                        double years, double volatility, double risk_free);
OptionAnalytics tdx_option_analytics(
    bool futures_model, bool american, bool call, double divisor,
    double underlying, double strike, double years, double volatility,
    double risk_free);
double tdx_implied_volatility(bool futures_model, bool american, bool call,
                              double divisor, double underlying, double strike,
                              double years, double initial_volatility,
                              double risk_free, double option_price);

Json fetch_option_volatility_document(
    const std::string& market, const std::string& wire_code,
    const std::string& option_name = {}, const std::string& date = {},
    const std::string& expiry = {}, int lookback = 60,
    double risk_free = 0.0187,
    std::optional<double> option_price_override = std::nullopt,
    bool refresh_catalog = false, int cache_ttl_seconds = 300,
    int timeout_ms = 30000,
    const std::filesystem::path& root = {});
Json analyze_option_chain_document(
    const Json& option_catalog, const Json& quote_batch,
    double underlying_price, const std::string& calculation_date,
    const std::string& expiry, double historical_volatility,
    double risk_free = 0.0187);
Json fetch_option_chain_document(
    const std::filesystem::path& root, const std::string& market,
    const std::string& contract, const std::string& expiry = {},
    int lookback = 60, double risk_free = 0.0187, int limit = 500,
    bool refresh_catalog = false, int cache_ttl_seconds = 300,
    int timeout_ms = 30000);

int command_market_options(const std::vector<std::string>& args);
int command_market_option_expiry(const std::vector<std::string>& args);
int command_market_option_volatility(const std::vector<std::string>& args);
int command_market_option_chain(const std::vector<std::string>& args);

}  // namespace tdx
