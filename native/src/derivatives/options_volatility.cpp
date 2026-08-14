#include "options_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tdx {

using namespace option_detail;

Json fetch_option_volatility_document(
    const std::string& market_text, const std::string& wire_code_text,
    const std::string& option_name, const std::string& requested_date,
    const std::string& expiry_text, int lookback, double risk_free,
    std::optional<double> option_price_override, bool refresh_catalog,
    int cache_ttl_seconds, int timeout_ms, const fs::path& root) {
    const int market_id = option_market_id(market_text);
    auto wire_code = upper_ascii(wire_code_text);
    if (!valid_expansion_code(wire_code))
        throw Error("option wire code must contain 1..9 ASCII letters, digits, or internal spaces");
    if (lookback < 2 || lookback > 800) throw Error("lookback must be in the range 2..800");
    if (!std::isfinite(risk_free) || risk_free < -1.0 || risk_free > 1.0)
        throw Error("risk-free rate is outside the supported range");
    std::optional<OptionInstrument> option;
    bool catalog_used = false;
    if (!trim(option_name).empty()) {
        option = parse_option_instrument(market_id, wire_code, option_name);
    } else {
        const auto catalog = cached_full_option_catalog(refresh_catalog,
                                                        cache_ttl_seconds, timeout_ms);
        option = find_option_in_catalog(catalog, market_id, wire_code);
        catalog_used = true;
    }
    if (!option) throw Error("selected security is absent from the active TDX option catalog");

    const int page_size = std::min(800, std::max(120, lookback + 20));
    const auto option_bars = fetch_kline_document(
        std::to_string(option->market_id), option->wire_code, "stock", "day",
        1, page_size, 0, "all", timeout_ms);
    const auto& option_bar = select_bar(option_bars.at("bars"), requested_date);
    const auto calculation_date = option_bar.at("date").as_string();
    double selected_option_price = option_price_override.value_or(option_bar.at("close").as_number());
    if (!std::isfinite(selected_option_price) || selected_option_price <= 0.0)
        throw Error("selected option price must be positive");

    const auto underlying_bars = fetch_kline_document(
        std::to_string(option->underlying_market_id), option->underlying_code,
        "stock", "day", 1, page_size, 0, "all", timeout_ms);
    std::vector<double> closes;
    const Json* underlying_bar = nullptr;
    for (const auto& bar : underlying_bars.at("bars").as_array()) {
        if (bar.at("date").as_string() > calculation_date) continue;
        underlying_bar = &bar;
        closes.push_back(bar.at("close").as_number());
    }
    if (!underlying_bar || closes.size() < 2)
        throw Error("underlying daily K-line history is insufficient for volatility");
    if (closes.size() > static_cast<std::size_t>(lookback))
        closes.erase(closes.begin(), closes.end() - lookback);
    const double historical = tdx_historical_volatility(closes);
    const double underlying_price = underlying_bar->at("close").as_number();

    Json result = Json::object();
    result["schema"] = "tdx-option-volatility-v1";
    result["option"] = option_instrument_document(*option);
    result["calculation_date"] = calculation_date;
    result["option_price"] = selected_option_price;
    result["option_price_source"] = option_price_override ? "override" : "daily_close";
    result["underlying_price"] = underlying_price;
    result["underlying_price_source"] = "daily_close";
    result["lookback_requested"] = lookback;
    result["lookback_used"] = static_cast<std::uint64_t>(closes.size());
    result["historical_volatility"] = historical;
    result["historical_volatility_percent"] = historical * 100.0;
    result["annualization_days"] = 250;
    result["risk_free"] = risk_free;
    result["risk_free_source"] = risk_free == 0.0187 ? "TdxW_default_NORISK_RATE" : "request";
    result["catalog_lookup_used"] = catalog_used;
    result["model_source"] = "TQQCalc.dll/TdxW selector 35 reconstruction";
    result["transport"] = "tdx-7727-0x23ff";

    std::string selected_expiry = expiry_text;
    Json expiry_resolution;
    std::string expiry_resolution_error;
    if (trim(selected_expiry).empty()) {
        try {
            expiry_resolution = resolve_option_expiry_document(root, *option);
            const auto* resolved = object_value(expiry_resolution, "expiry");
            if (resolved && resolved->is_string()) selected_expiry = resolved->as_string();
        } catch (const std::exception& error) {
            expiry_resolution_error = error.what();
        }
    }
    const auto expiry = normalize_date(selected_expiry);
    if (expiry.empty()) {
        result["expiry"] = Json(nullptr);
        result["expiry_source"] = "unavailable";
        if (expiry_resolution.is_object() && expiry_resolution.size())
            result["expiry_resolution"] = expiry_resolution;
        if (!expiry_resolution_error.empty())
            result["expiry_resolution_error"] = expiry_resolution_error;
        result["implied_volatility"] = Json(nullptr);
        result["implied_volatility_percent"] = Json(nullptr);
        result["implied_status"] = "expiry_required";
        result["implied_message"] = "Exact expiry could not be resolved from local TDX rules; pass expiry explicitly.";
        return result;
    }
    const auto calendar_days = civil_days(expiry) - civil_days(calculation_date);
    if (calendar_days < 0) throw Error("expiry precedes the selected option bar");
    const double years = static_cast<double>(calendar_days + 1) / 365.0;
    const double initial = historical > 0.000001 ? historical : 0.2;
    const double implied = tdx_implied_volatility(
        option->futures_model, option->american, option->call, 1.0,
        underlying_price, option->strike, years, initial, risk_free,
        selected_option_price);
    const auto analytics = tdx_option_analytics(
        option->futures_model, option->american, option->call, 1.0,
        underlying_price, option->strike, years, implied, risk_free);
    result["expiry"] = expiry;
    result["expiry_source"] = trim(expiry_text).empty()
        ? "TdxW-code2name_qq/neednote" : "request";
    if (expiry_resolution.is_object() && expiry_resolution.size())
        result["expiry_resolution"] = expiry_resolution;
    result["calendar_days_to_expiry"] = static_cast<std::int64_t>(calendar_days);
    result["time_to_expiry_years"] = years;
    result["day_count"] = "calendar_days_plus_one/365";
    result["initial_volatility"] = initial;
    result["implied_volatility"] = implied;
    result["implied_volatility_percent"] = implied * 100.0;
    result["model_price"] = analytics.price;
    result["delta"] = analytics.delta;
    result["gamma"] = analytics.gamma;
    result["theta"] = analytics.theta;
    result["vega"] = analytics.vega;
    result["rho"] = analytics.rho;
    result["greeks_volatility_source"] = "implied_volatility";
    result["greeks_source"] = "TQQCalc_Index reconstruction";
    result["volatility_spread"] = implied - historical;
    result["volatility_spread_percent"] = (implied - historical) * 100.0;
    result["implied_status"] = "calculated";
    return result;
}

}  // namespace tdx
