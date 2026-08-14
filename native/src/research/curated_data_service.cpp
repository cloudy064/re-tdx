#include "tdx/curated_data.hpp"
#include "tdx/curated_data_internal.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <set>

namespace tdx {

using detail::curated_data::parsed_market;
using detail::curated_data::digits;
using detail::curated_data::text_value;
using detail::curated_data::field;
using detail::curated_data::now_text;

Json CuratedDataService::query(const CuratedDataQuery& options) {
    const std::set<std::string> views{
        "all", "media-entertainment", "low-valuation-smallcap",
        "dividend-fundraising", "buyback-statistics", "high-dividend",
        "hk-performance", "high-refinancing-lending", "below-book-soe"};
    if (!views.count(options.view))
        throw Error("unsupported curated-data view");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = parsed_market(options.market);
        if (selected_market < 0)
            throw Error("market must be sz/sh/bj/hk or 0/1/2/31");
        if (!digits(options.code, selected_market == 31 ? 5 : 6))
            throw Error(selected_market == 31
                ? "Hong Kong code must contain five digits"
                : "code must contain six digits");
    }

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "kind") != options.view)
            continue;
        if (selected_market >= 0) {
            const auto* security = field(row, "security");
            if (!security || security->is_null()) continue;
            const auto row_market =
                static_cast<int>(security->at("market_id").as_number());
            const bool market_matches = row_market == selected_market ||
                (selected_market == 31 &&
                 (row_market == 31 || row_market == 48 || row_market == 49));
            if (!market_matches || security->at("code").as_string() != options.code)
                continue;
        }
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        records.push_back(row);
    }

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    std::string earliest, latest, lending_latest;
    std::uint64_t valid_lending = 0;
    for (const auto& row : records.as_array()) {
        const auto kind = text_value(row, "kind");
        ++counts[kind];
        const auto* security = field(row, "security");
        if (security && !security->is_null())
            securities.insert(security->at("security_id").as_string());
        const auto date = text_value(row, "date");
        if (!date.empty()) {
            if (earliest.empty() || date < earliest) earliest = date;
            if (date > latest) latest = date;
        }
        if (kind == "high-refinancing-lending") {
            if (date > lending_latest) lending_latest = date;
            const auto* has = field(row, "has_lending_data");
            if (has && has->is_bool() && has->as_bool()) ++valid_lending;
        }
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json summary = Json::object();
    summary["media_entertainment"] = counts["media-entertainment"];
    summary["low_valuation_smallcap"] = counts["low-valuation-smallcap"];
    summary["dividend_fundraising"] = counts["dividend-fundraising"];
    summary["buyback_statistics"] = counts["buyback-statistics"];
    summary["high_dividend"] = counts["high-dividend"];
    summary["hk_performance"] = counts["hk-performance"];
    summary["high_refinancing_lending"] =
        counts["high-refinancing-lending"];
    summary["below_book_soe"] = counts["below-book-soe"];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    summary["valid_refinancing_lending_rows"] = valid_lending;
    summary["refinancing_lending_latest_date"] = lending_latest;
    summary["earliest_date"] = earliest;
    summary["latest_date"] = latest;

    Json result = Json::object();
    result["schema"] = "tdx-market-curated-data-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = selected_market < 0 ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "Eight client-curated BigData tables are normalized without treating them "
        "as live quotes. The high-refinancing-lending source is a historical snapshot; "
        "blank balances remain explicit. CFG 万元/万股/亿元 fields are also exposed "
        "in yuan/share base units, and client formulas are reproduced.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
