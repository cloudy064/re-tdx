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

Json analyze_option_chain_document(
    const Json& option_catalog, const Json& quote_batch,
    double underlying_price, const std::string& calculation_date_text,
    const std::string& expiry_text, double historical_volatility,
    double risk_free) {
    if (!option_catalog.is_object() || !object_value(option_catalog, "options") ||
        !option_catalog.at("options").is_array() || option_catalog.at("options").size() == 0)
        throw Error("option-chain analysis requires a non-empty normalized option catalog");
    if (!quote_batch.is_object() || !object_value(quote_batch, "quotes") ||
        !quote_batch.at("quotes").is_array())
        throw Error("option-chain analysis requires an expansion quote batch");
    if (!(underlying_price > 0.0) || !std::isfinite(underlying_price))
        throw Error("option-chain underlying price must be positive");
    if (!std::isfinite(historical_volatility) || historical_volatility < 0.0)
        throw Error("option-chain historical volatility is invalid");
    if (!std::isfinite(risk_free) || risk_free < -1.0 || risk_free > 1.0)
        throw Error("option-chain risk-free rate is outside the supported range");
    const auto calculation_date = normalize_date(calculation_date_text);
    const auto expiry = normalize_date(expiry_text);
    const auto calendar_days = civil_days(expiry) - civil_days(calculation_date);
    if (calendar_days < 0) throw Error("option-chain expiry precedes the calculation date");
    const double years = static_cast<double>(calendar_days + 1) / 365.0;
    const double initial_volatility = historical_volatility > 0.000001
        ? historical_volatility : 0.2;

    struct StrikePair { Json call{nullptr}; Json put{nullptr}; };
    struct PainLeg { bool call{}; double strike{}; std::uint64_t open_interest{}; };
    std::map<double, StrikePair> strikes;
    std::vector<PainLeg> pain_legs;
    std::uint64_t call_volume = 0, put_volume = 0;
    std::uint64_t call_open_interest = 0, put_open_interest = 0;
    std::uint64_t quoted_count = 0, calculated_count = 0;
    double call_iv_weighted = 0.0, put_iv_weighted = 0.0;
    double call_iv_weight = 0.0, put_iv_weight = 0.0;
    std::optional<OptionInstrument> representative;

    for (const auto& row : option_catalog.at("options").as_array()) {
        const int market_id = static_cast<int>(json_number_or(row, "market_id", -1.0));
        const auto* code_value = object_value(row, "code");
        const auto* name_value = object_value(row, "name");
        if (!code_value || !code_value->is_string() || !name_value || !name_value->is_string())
            continue;
        auto option = parse_option_instrument(
            market_id, code_value->as_string(), name_value->as_string());
        if (!option) continue;
        if (!representative) representative = option;
        Json leg = option_instrument_document(*option);
        leg["quote_status"] = "unavailable";
        leg["last_price"] = Json(nullptr);
        leg["bid_price"] = Json(nullptr);
        leg["ask_price"] = Json(nullptr);
        leg["mark_price"] = Json(nullptr);
        leg["implied_volatility"] = Json(nullptr);
        leg["implied_volatility_percent"] = Json(nullptr);
        leg["model_price"] = Json(nullptr);
        leg["delta"] = Json(nullptr); leg["gamma"] = Json(nullptr);
        leg["theta"] = Json(nullptr); leg["vega"] = Json(nullptr);
        leg["rho"] = Json(nullptr);
        leg["volume"] = static_cast<std::uint64_t>(0);
        leg["open_interest"] = static_cast<std::uint64_t>(0);
        leg["strike_over_underlying"] = option->strike / underlying_price;
        const Json* quote = find_expansion_quote(
            quote_batch, option->market_id, option->wire_code);
        if (quote) {
            ++quoted_count;
            const double last = json_number_or(*quote, "price");
            const double pre_close = json_number_or(*quote, "pre_close");
            const double bid = top_of_book(*quote, "bids");
            const double ask = top_of_book(*quote, "asks");
            double mark = 0.0;
            std::string mark_source = "unavailable";
            if (last > 0.0) { mark = last; mark_source = "tdx_current_price"; }
            else if (bid > 0.0 && ask > 0.0) {
                mark = (bid + ask) * 0.5; mark_source = "bid_ask_midpoint";
            } else if (pre_close > 0.0) {
                mark = pre_close; mark_source = "previous_settlement";
            }
            const auto volume = static_cast<std::uint64_t>(
                std::max(0.0, json_number_or(*quote, "volume")));
            const auto open_interest = static_cast<std::uint64_t>(
                std::max(0.0, json_number_or(*quote, "open_interest")));
            leg["quote_status"] = mark > 0.0 ? "priced" : "no_positive_price";
            leg["last_price"] = last > 0.0 ? Json(last) : Json(nullptr);
            leg["previous_settlement"] = pre_close > 0.0 ? Json(pre_close) : Json(nullptr);
            leg["bid_price"] = bid > 0.0 ? Json(bid) : Json(nullptr);
            leg["ask_price"] = ask > 0.0 ? Json(ask) : Json(nullptr);
            leg["mark_price"] = mark > 0.0 ? Json(mark) : Json(nullptr);
            leg["mark_price_source"] = mark_source;
            leg["volume"] = volume;
            leg["open_interest"] = open_interest;
            const double intrinsic = option->call
                ? std::max(underlying_price - option->strike, 0.0)
                : std::max(option->strike - underlying_price, 0.0);
            leg["intrinsic_value"] = intrinsic;
            leg["time_value"] = mark > 0.0 ? Json(mark - intrinsic) : Json(nullptr);
            if (option->call) {
                call_volume += volume; call_open_interest += open_interest;
            } else {
                put_volume += volume; put_open_interest += open_interest;
            }
            pain_legs.push_back({option->call, option->strike, open_interest});
            if (mark > 0.0) {
                const double implied = tdx_implied_volatility(
                    option->futures_model, option->american, option->call, 1.0,
                    underlying_price, option->strike, years, initial_volatility,
                    risk_free, mark);
                const auto analytics = tdx_option_analytics(
                    option->futures_model, option->american, option->call, 1.0,
                    underlying_price, option->strike, years, implied, risk_free);
                leg["implied_volatility"] = implied;
                leg["implied_volatility_percent"] = implied * 100.0;
                leg["model_price"] = analytics.price;
                leg["delta"] = analytics.delta; leg["gamma"] = analytics.gamma;
                leg["theta"] = analytics.theta; leg["vega"] = analytics.vega;
                leg["rho"] = analytics.rho;
                leg["implied_status"] = "calculated";
                ++calculated_count;
                if (open_interest) {
                    if (option->call) {
                        call_iv_weighted += implied * static_cast<double>(open_interest);
                        call_iv_weight += static_cast<double>(open_interest);
                    } else {
                        put_iv_weighted += implied * static_cast<double>(open_interest);
                        put_iv_weight += static_cast<double>(open_interest);
                    }
                }
            } else {
                leg["implied_status"] = "price_unavailable";
            }
        }
        auto& strike = strikes[option->strike];
        if (option->call) strike.call = std::move(leg);
        else strike.put = std::move(leg);
    }
    if (!representative || strikes.empty())
        throw Error("option-chain catalog contained no parseable option contracts");

    double atm_strike = strikes.begin()->first;
    for (const auto& [strike, pair] : strikes) {
        (void)pair;
        if (std::abs(strike - underlying_price) < std::abs(atm_strike - underlying_price))
            atm_strike = strike;
    }
    Json strike_rows = Json::array();
    for (auto& [strike, pair] : strikes) {
        Json item = Json::object();
        item["strike"] = strike;
        item["distance_to_underlying"] = strike - underlying_price;
        item["call"] = std::move(pair.call);
        item["put"] = std::move(pair.put);
        strike_rows.push_back(std::move(item));
    }

    Json max_pain = Json(nullptr);
    if (!pain_legs.empty()) {
        double selected_strike = strikes.begin()->first;
        long double selected_payout = std::numeric_limits<long double>::infinity();
        for (const auto& [settlement, pair] : strikes) {
            (void)pair;
            long double payout = 0.0;
            for (const auto& leg : pain_legs) {
                const double intrinsic = leg.call
                    ? std::max(settlement - leg.strike, 0.0)
                    : std::max(leg.strike - settlement, 0.0);
                payout += static_cast<long double>(intrinsic) * leg.open_interest;
            }
            if (payout < selected_payout) {
                selected_payout = payout; selected_strike = settlement;
            }
        }
        max_pain = Json::object();
        max_pain["strike"] = selected_strike;
        max_pain["aggregate_intrinsic_open_interest"] = static_cast<double>(selected_payout);
        max_pain["method"] = "minimum_open_interest_weighted_intrinsic_value";
    }

    Json summary = Json::object();
    summary["strike_count"] = static_cast<std::uint64_t>(strikes.size());
    summary["contract_count"] = static_cast<std::uint64_t>(
        option_catalog.at("options").size());
    summary["quoted_count"] = quoted_count;
    summary["calculated_iv_count"] = calculated_count;
    summary["call_volume"] = call_volume; summary["put_volume"] = put_volume;
    summary["call_open_interest"] = call_open_interest;
    summary["put_open_interest"] = put_open_interest;
    summary["put_call_volume_ratio"] = call_volume
        ? Json(static_cast<double>(put_volume) / static_cast<double>(call_volume)) : Json(nullptr);
    summary["put_call_open_interest_ratio"] = call_open_interest
        ? Json(static_cast<double>(put_open_interest) /
               static_cast<double>(call_open_interest)) : Json(nullptr);
    summary["call_open_interest_weighted_iv"] = call_iv_weight > 0.0
        ? Json(call_iv_weighted / call_iv_weight) : Json(nullptr);
    summary["put_open_interest_weighted_iv"] = put_iv_weight > 0.0
        ? Json(put_iv_weighted / put_iv_weight) : Json(nullptr);
    summary["max_pain"] = std::move(max_pain);

    Json atm = Json::object();
    atm["strike"] = atm_strike;
    atm["distance"] = atm_strike - underlying_price;
    const auto atm_row = std::find_if(strike_rows.as_array().begin(), strike_rows.as_array().end(),
        [&](const Json& row) { return row.at("strike").as_number() == atm_strike; });
    if (atm_row != strike_rows.as_array().end()) {
        atm["call"] = atm_row->at("call"); atm["put"] = atm_row->at("put");
        if (!atm_row->at("call").is_null() && !atm_row->at("put").is_null()) {
            const auto* call_iv = object_value(atm_row->at("call"), "implied_volatility");
            const auto* put_iv = object_value(atm_row->at("put"), "implied_volatility");
            atm["put_minus_call_iv"] = call_iv && put_iv && call_iv->is_number() && put_iv->is_number()
                ? Json(put_iv->as_number() - call_iv->as_number()) : Json(nullptr);
        }
    }

    Json result = Json::object();
    result["schema"] = "tdx-option-chain-v1";
    result["market_id"] = representative->market_id;
    result["market"] = option_market_name(representative->market_id);
    result["contract"] = representative->contract;
    result["underlying_market_id"] = representative->underlying_market_id;
    result["underlying_code"] = representative->underlying_code;
    result["underlying_security"] = std::to_string(representative->underlying_market_id) +
                                      ":" + representative->underlying_code;
    result["underlying_price"] = underlying_price;
    result["calculation_date"] = calculation_date;
    result["expiry"] = expiry;
    result["calendar_days_to_expiry"] = static_cast<std::int64_t>(calendar_days);
    result["time_to_expiry_years"] = years;
    result["historical_volatility"] = historical_volatility;
    result["historical_volatility_percent"] = historical_volatility * 100.0;
    result["risk_free"] = risk_free;
    result["at_the_money"] = std::move(atm);
    result["summary"] = std::move(summary);
    result["strikes"] = std::move(strike_rows);
    result["model_source"] = "TQQCalc.dll/TdxW IVOLAT reconstruction";
    result["transport"] = "tdx-7727-0x23fa-persistent-batch";
    if (const auto* endpoint = object_value(quote_batch, "endpoint"))
        result["endpoint"] = *endpoint;
    if (const auto* server = object_value(quote_batch, "server_name"))
        result["server_name"] = *server;
    return result;
}

Json fetch_option_chain_document(
    const fs::path& root, const std::string& market_text,
    const std::string& contract_text, const std::string& expiry_text,
    int lookback, double risk_free, int limit, bool refresh_catalog,
    int cache_ttl_seconds, int timeout_ms) {
    const int market_id = option_market_id(market_text);
    const auto contract = upper_ascii(trim(contract_text));
    if (contract.empty()) throw Error("option-chain contract is required");
    if (lookback < 2 || lookback > 800) throw Error("lookback must be in the range 2..800");
    if (limit < 2 || limit > 2000) throw Error("option-chain limit must be in the range 2..2000");
    auto catalog = fetch_option_catalog_document(
        market_id, {}, contract, "all", {}, limit, refresh_catalog,
        cache_ttl_seconds, timeout_ms);
    Json exact_rows = Json::array();
    for (const auto& row : catalog.at("options").as_array())
        if (upper_ascii(row.at("contract").as_string()) == contract)
            exact_rows.push_back(row);
    if (exact_rows.size() < 2)
        throw Error("active TDX option catalog has no complete chain for the requested contract");
    catalog["options"] = std::move(exact_rows);
    catalog["matched"] = static_cast<std::uint64_t>(catalog.at("options").size());
    catalog["returned"] = static_cast<std::uint64_t>(catalog.at("options").size());
    const auto& first_row = catalog.at("options").as_array().front();
    const auto representative = parse_option_instrument(
        market_id, first_row.at("code").as_string(), first_row.at("name").as_string());
    if (!representative) throw Error("option-chain representative contract is invalid");

    Json expiry_resolution;
    std::string expiry = normalize_date(expiry_text);
    if (expiry.empty()) {
        expiry_resolution = resolve_option_expiry_document(root, *representative);
        const auto* value = object_value(expiry_resolution, "expiry");
        if (value && value->is_string()) expiry = value->as_string();
    }
    if (expiry.empty())
        throw Error("exact option-chain expiry is unavailable; pass expiry explicitly");

    const auto calculation_date = local_calendar_date();
    const int page_size = std::min(800, std::max(120, lookback + 20));
    const auto underlying_bars = fetch_kline_document(
        std::to_string(representative->underlying_market_id),
        representative->underlying_code, "stock", "day", 1, page_size,
        0, "all", timeout_ms);
    std::vector<double> closes;
    const Json* latest_underlying_bar = nullptr;
    for (const auto& bar : underlying_bars.at("bars").as_array()) {
        if (bar.at("date").as_string() > calculation_date) continue;
        latest_underlying_bar = &bar;
        closes.push_back(bar.at("close").as_number());
    }
    if (!latest_underlying_bar || closes.size() < 2)
        throw Error("underlying daily K-line history is insufficient for option-chain analysis");
    if (closes.size() > static_cast<std::size_t>(lookback))
        closes.erase(closes.begin(), closes.end() - lookback);
    const double historical = tdx_historical_volatility(closes);

    std::vector<std::pair<int, std::string>> securities;
    securities.reserve(catalog.at("options").size() + 1);
    securities.push_back({representative->underlying_market_id,
                          representative->underlying_code});
    for (const auto& row : catalog.at("options").as_array())
        securities.push_back({market_id, row.at("code").as_string()});
    const auto quotes = fetch_expansion_quotes_document(securities, timeout_ms);
    double underlying_price = latest_underlying_bar->at("close").as_number();
    std::string underlying_price_source = "latest_daily_close";
    if (const auto* quote = find_expansion_quote(
            quotes, representative->underlying_market_id,
            representative->underlying_code)) {
        const double current = json_number_or(*quote, "price");
        if (current > 0.0) {
            underlying_price = current;
            underlying_price_source = "tdx_current_price";
        }
    }
    auto result = analyze_option_chain_document(
        catalog, quotes, underlying_price, calculation_date, expiry,
        historical, risk_free);
    result["underlying_price_source"] = underlying_price_source;
    result["historical_close_date"] = latest_underlying_bar->at("date");
    result["lookback_requested"] = lookback;
    result["lookback_used"] = static_cast<std::uint64_t>(closes.size());
    result["catalog_cache_ttl_seconds"] = cache_ttl_seconds;
    result["catalog_refresh"] = refresh_catalog;
    result["expiry_source"] = trim(expiry_text).empty()
        ? "TdxW-code2name_qq/neednote" : "request";
    if (expiry_resolution.is_object() && expiry_resolution.size())
        result["expiry_resolution"] = std::move(expiry_resolution);
    result["calculation_date_source"] = "local_calendar_date_for_current_0x23fa_quotes";
    return result;
}

}  // namespace tdx
