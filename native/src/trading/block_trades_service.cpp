#include "block_trades_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace tdx {
namespace bt_detail = block_trade_detail;

Json BlockTradeService::query(const BlockTradeQuery& options) {
    if (!bt_detail::valid_view(options.view))
        throw Error("view must be trades, intentions, brokers, or industries");
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("detail_limit must be in 1..5000");
    if (options.master_cache_ttl_seconds < 0 ||
        options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 ||
        options.detail_cache_ttl_seconds > 86400)
        throw Error("block-trade cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    if (security_mode && !bt_detail::six_digits(options.code))
        throw Error("code must contain six digits");
    if (!options.month.empty() && !bt_detail::valid_month(options.month))
        throw Error("month must use YYYY-MM");
    if (!options.industry.empty() && !bt_detail::six_digits(options.industry))
        throw Error("industry must contain six digits");
    if (!options.broker_id.empty() &&
        (options.broker_id.size() > 32 ||
         !std::all_of(
             options.broker_id.begin(), options.broker_id.end(), [](char ch) {
                 return (ch >= '0' && ch <= '9') ||
                        (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z');
             })))
        throw Error("broker_id must be an alphanumeric identifier");
    const auto& period = bt_detail::broker_period(options.period);
    const int selected_market = security_mode
        ? bt_detail::canonical_market_id(options.market) : 0;
    auto core = fetch_core(options);

    Json trades = Json::array(), intentions = Json::array();
    const auto security_matches = [&](const Json& row) {
        if (!security_mode) return true;
        const auto& security = row.at("security");
        return static_cast<int>(security.at("market_id").as_number()) ==
                   selected_market &&
               bt_detail::text_value(security, "code") == options.code;
    };
    Json matching_trades = Json::array(), matching_intentions = Json::array();
    for (const auto& row : core.document.at("trades").as_array())
        if (security_matches(row)) matching_trades.push_back(row);
    for (const auto& row : core.document.at("intentions").as_array())
        if (security_matches(row)) matching_intentions.push_back(row);
    if (options.view == "trades" || security_mode)
        trades = bt_detail::limited_filtered(
            matching_trades, options.query, options.limit);
    if (options.view == "intentions" || security_mode)
        intentions = bt_detail::limited_filtered(
            matching_intentions, options.query, options.limit);

    Json brokers = Json::array(), industries = Json::array();
    Json industry_securities = Json::array();
    Json security_history = Json::array(), security_intentions = Json::array();
    Json broker_trades = Json::array(), errors = Json::array();
    Json extra_sources = Json::array();
    bool any_detail_refreshed = false;
    int greatest_detail_age = 0;
    Json selected_broker = Json(nullptr), selected_industry = Json(nullptr);

    const auto record_fetch = [&](const FetchResult& fetched) {
        extra_sources.push_back(bt_detail::source_summary(fetched.document));
        any_detail_refreshed = any_detail_refreshed || fetched.refreshed;
        greatest_detail_age = std::max(
            greatest_detail_age, fetched.age_seconds);
    };
    const auto add_error = [&](const std::string& resource,
                               const std::string& message) {
        Json failure = Json::object();
        failure["resource"] = resource;
        failure["message"] = message;
        errors.push_back(std::move(failure));
    };

    if (options.view == "brokers") {
        auto fetched = fetch_resource(std::string(period.resource), options);
        record_fetch(fetched);
        auto normalized = normalize_block_trade_broker_rows(
            fetched.document.at("rows"), options.period);
        if (!options.broker_id.empty()) {
            Json selected = Json::array();
            for (const auto& row : normalized.as_array())
                if (bt_detail::text_value(row, "broker_id") ==
                    options.broker_id)
                    selected.push_back(row);
            normalized = std::move(selected);
        }
        brokers = bt_detail::limited_filtered(
            normalized, options.query, options.limit);
        if (brokers.size()) selected_broker = brokers.as_array().front();
        if (!options.broker_id.empty()) {
            const auto resource = std::string(period.detail_namespace) + "/" +
                                  options.broker_id + ".jsn";
            try {
                auto detail = fetch_resource(resource, options);
                record_fetch(detail);
                broker_trades = normalize_block_trade_broker_detail_rows(
                    detail.document.at("rows"), securities_);
                if (static_cast<int>(broker_trades.size()) >
                    options.detail_limit)
                    broker_trades.as_array().resize(
                        static_cast<std::size_t>(options.detail_limit));
            } catch (const std::exception& error) {
                add_error(resource, error.what());
            }
        }
    }

    std::string selected_month = options.month;
    if (selected_month.empty() && core.document.at("monthly").size())
        selected_month = bt_detail::text_value(
            core.document.at("monthly").as_array().front(), "month");
    if (options.view == "industries") {
        const auto resource = "dzjy1/" + selected_month + ".jsn";
        try {
            auto fetched = fetch_resource(resource, options);
            record_fetch(fetched);
            auto normalized = normalize_block_trade_industry_rows(
                fetched.document.at("rows"));
            if (!options.industry.empty()) {
                Json selected = Json::array();
                for (const auto& row : normalized.as_array())
                    if (bt_detail::text_value(row, "industry_id") ==
                        options.industry)
                        selected.push_back(row);
                normalized = std::move(selected);
            }
            industries = bt_detail::limited_filtered(
                normalized, options.query, options.limit);
            if (industries.size())
                selected_industry = industries.as_array().front();
            if (!options.industry.empty() && !selected_industry.is_null()) {
                const auto detail_resource = "dzjy2/" +
                    bt_detail::text_value(selected_industry, "detail_id") +
                    ".jsn";
                try {
                    auto detail = fetch_resource(detail_resource, options);
                    record_fetch(detail);
                    industry_securities =
                        normalize_block_trade_industry_security_rows(
                            detail.document.at("rows"), securities_);
                    if (static_cast<int>(industry_securities.size()) >
                        options.detail_limit)
                        industry_securities.as_array().resize(
                            static_cast<std::size_t>(options.detail_limit));
                } catch (const std::exception& error) {
                    add_error(detail_resource, error.what());
                }
            }
        } catch (const std::exception& error) {
            add_error(resource, error.what());
        }
    }

    if (security_mode && (options.include_details || security_mode)) {
        const auto key = std::to_string(selected_market) + options.code;
        const auto history_resource = "dzjy3/" + key + ".jsn";
        try {
            auto fetched = fetch_resource(history_resource, options);
            record_fetch(fetched);
            security_history = normalize_block_trade_history_rows(
                fetched.document.at("rows"));
            if (static_cast<int>(security_history.size()) >
                options.detail_limit)
                security_history.as_array().resize(
                    static_cast<std::size_t>(options.detail_limit));
        } catch (const std::exception& error) {
            add_error(history_resource, error.what());
        }
        const auto intentions_resource = "dzjy13/" + key + ".jsn";
        try {
            auto fetched = fetch_resource(intentions_resource, options);
            record_fetch(fetched);
            security_intentions = normalize_block_trade_intention_rows(
                fetched.document.at("rows"), securities_, false);
            if (static_cast<int>(security_intentions.size()) >
                options.detail_limit)
                security_intentions.as_array().resize(
                    static_cast<std::size_t>(options.detail_limit));
        } catch (const std::exception& error) {
            add_error(intentions_resource, error.what());
        }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-block-trades-native-v1";
    result["generated_at"] = bt_detail::now_text();
    result["view"] = options.view;
    result["mode"] = security_mode ? "security" :
        !options.broker_id.empty() ? "broker" :
        !options.industry.empty() ? "industry" : "catalog";
    result["monthly"] = core.document.at("monthly");
    result["trades"] = std::move(trades);
    result["intentions"] = std::move(intentions);
    result["brokers"] = std::move(brokers);
    result["industries"] = std::move(industries);
    result["industry_securities"] = std::move(industry_securities);
    result["security_history"] = std::move(security_history);
    result["security_intentions"] = std::move(security_intentions);
    result["broker_trades"] = std::move(broker_trades);
    result["selected_security"] = security_mode
        ? bt_detail::security_document(
              selected_market, options.code, securities_) : Json(nullptr);
    result["selected_broker"] = std::move(selected_broker);
    result["selected_industry"] = std::move(selected_industry);
    result["summary"] = core.document.at("summary");
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["query"] = options.query;
    filters["period"] = options.period;
    filters["month"] = selected_month;
    filters["industry"] = options.industry;
    filters["broker_id"] = options.broker_id;
    result["filters"] = std::move(filters);
    Json sources = core.document.at("sources");
    for (const auto& source : extra_sources.as_array())
        sources.push_back(source);
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = core.refreshed;
    cache["master_age_seconds"] = core.age_seconds;
    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["detail_refreshed"] = any_detail_refreshed;
    cache["detail_age_seconds"] = greatest_detail_age;
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["monthly"] = static_cast<std::uint64_t>(
        result.at("monthly").size());
    counts["trades"] = static_cast<std::uint64_t>(
        result.at("trades").size());
    counts["intentions"] = static_cast<std::uint64_t>(
        result.at("intentions").size());
    counts["brokers"] = static_cast<std::uint64_t>(
        result.at("brokers").size());
    counts["industries"] = static_cast<std::uint64_t>(
        result.at("industries").size());
    counts["industry_securities"] = static_cast<std::uint64_t>(
        result.at("industry_securities").size());
    counts["security_history"] = static_cast<std::uint64_t>(
        result.at("security_history").size());
    counts["security_intentions"] = static_cast<std::uint64_t>(
        result.at("security_intentions").size());
    counts["broker_trades"] = static_cast<std::uint64_t>(
        result.at("broker_trades").size());
    counts["detail_errors"] = static_cast<std::uint64_t>(
        result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

}  // namespace tdx
