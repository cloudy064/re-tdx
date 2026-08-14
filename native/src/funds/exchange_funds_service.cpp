#include "exchange_funds_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace tdx {
namespace detail = exchange_fund_detail;

Json ExchangeFundService::query(const ExchangeFundQuery& options) {
    if (!detail::valid_view(options.view))
        throw Error("view must be all, etf-performance, etf-share-ranking, etf-scale-flow, "
                    "commodity-etf, cash-arbitrage, cash-yield, lof, "
                    "closed-fund, cash-management-calendar, reits-issued, "
                    "or reits-pipeline");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = detail::parsed_market(options.market);
        if (selected_market < 0)
            throw Error("market must be sz/sh/bj or 0/1/2");
        if (!detail::digits(options.code))
            throw Error("code must contain six digits");
    }

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto selected_kind = detail::view_kind(options.view);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (!selected_kind.empty() &&
            detail::text_value(row, "kind") != selected_kind)
            continue;
        const auto& security = row.at("security");
        if (selected_market >= 0 &&
            (static_cast<int>(security.at("market_id").as_number()) !=
                 selected_market ||
             security.at("code").as_string() != options.code))
            continue;
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
            continue;
        records.push_back(row);
    }

    std::stable_sort(
        records.as_array().begin(), records.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_kind = detail::text_value(left, "kind");
            const auto right_kind = detail::text_value(right, "kind");
            const auto left_rank = detail::kind_rank(left_kind);
            const auto right_rank = detail::kind_rank(right_kind);
            if (left_rank != right_rank) return left_rank < right_rank;
            if (left_kind == "etf-performance") {
                const auto left_amount = detail::number_value(
                    left, "turnover_5d_yuan").value_or(0);
                const auto right_amount = detail::number_value(
                    right, "turnover_5d_yuan").value_or(0);
                if (left_amount != right_amount)
                    return left_amount > right_amount;
            } else if (left_kind == "etf-share-ranking") {
                const auto left_flow = detail::number_value(
                    left, "net_inflow_yuan").value_or(-1e100);
                const auto right_flow = detail::number_value(
                    right, "net_inflow_yuan").value_or(-1e100);
                if (left_flow != right_flow) return left_flow > right_flow;
                const auto left_shares = detail::number_value(
                    left, "latest_shares").value_or(0);
                const auto right_shares = detail::number_value(
                    right, "latest_shares").value_or(0);
                if (left_shares != right_shares)
                    return left_shares > right_shares;
            } else if (left_kind == "cash-arbitrage" ||
                       left_kind == "cash-yield") {
                const auto left_yield = detail::number_value(
                    left, "seven_day_annualized_pct").value_or(-1e100);
                const auto right_yield = detail::number_value(
                    right, "seven_day_annualized_pct").value_or(-1e100);
                if (left_yield != right_yield)
                    return left_yield > right_yield;
            } else {
                const auto left_date = detail::text_value(
                    left, "snapshot_date");
                const auto right_date = detail::text_value(
                    right, "snapshot_date");
                if (left_date != right_date) return left_date > right_date;
            }
            return left.at("security").at("security_id").as_string() <
                   right.at("security").at("security_id").as_string();
        });

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    double daily_turnover = 0.0, five_day_turnover = 0.0;
    double etf_latest_shares = 0.0, etf_net_inflow_yuan = 0.0;
    std::uint64_t etf_nonzero_net_inflow = 0;
    for (const auto& row : records.as_array()) {
        ++counts[detail::text_value(row, "kind")];
        securities.insert(
            row.at("security").at("security_id").as_string());
        daily_turnover += detail::number_value(
            row, "turnover_yuan").value_or(0.0);
        five_day_turnover += detail::number_value(
            row, "turnover_5d_yuan").value_or(0.0);
        if (detail::text_value(row, "kind") == "etf-share-ranking") {
            etf_latest_shares += detail::number_value(
                row, "latest_shares").value_or(0.0);
            const auto flow = detail::number_value(
                row, "net_inflow_yuan").value_or(0.0);
            etf_net_inflow_yuan += flow;
            if (std::abs(flow) > 0.000001) ++etf_nonzero_net_inflow;
        }
    }
    const auto matched = records.size();
    while (static_cast<int>(records.size()) > options.limit)
        records.as_array().pop_back();

    Json quote_source = Json(nullptr);
    Json quote_errors = Json::array();
    bool quote_refreshed = false;
    int quote_age_seconds = 0;
    if (options.include_quotes && records.size()) {
        std::vector<std::string> requested;
        for (const auto& row : records.as_array()) {
            const auto& security = row.at("security");
            requested.push_back(
                security.at("market").as_string() + ":" +
                security.at("code").as_string());
        }
        try {
            const auto quotes = fetch_quotes(
                requested, options, quote_refreshed, quote_age_seconds);
            apply_exchange_fund_quotes(records, quotes.at("records"));
            quote_source = Json::object();
            for (const auto* key :
                 {"endpoint", "server_name", "requested", "received"})
                quote_source[key] = quotes.at(key);
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = "public-l1-snapshot";
            failure["message"] = error.what();
            quote_errors.push_back(std::move(failure));
        }
    }

    std::uint64_t configured_empty_sources = 0;
    for (const auto& source : master.at("sources").as_array())
        if (source.at("row_count").as_number() > 0 &&
            source.at("normalized_row_count").as_number() == 0)
            ++configured_empty_sources;

    Json summary = Json::object();
    summary["etf_performance"] = counts["etf-performance"];
    summary["etf_share_ranking"] = counts["etf-share-ranking"];
    summary["etf_scale_flow"] = counts["etf-scale-flow"];
    summary["commodity_etf"] = counts["commodity-etf"];
    summary["cash_arbitrage"] = counts["cash-arbitrage"];
    summary["cash_yield"] = counts["cash-yield"];
    summary["lof"] = counts["lof"];
    summary["closed_fund"] = counts["closed-fund"];
    summary["cash_management_calendar"] =
        counts["cash-management-calendar"];
    summary["reits_issued"] = counts["reit-issued"];
    summary["reits_pipeline"] = counts["reit-pipeline"];
    summary["unique_securities"] =
        static_cast<std::uint64_t>(securities.size());
    summary["daily_turnover_yuan"] = daily_turnover;
    summary["five_day_turnover_yuan"] = five_day_turnover;
    summary["etf_share_ranking_latest_shares"] = etf_latest_shares;
    summary["etf_share_ranking_net_inflow_yuan"] = etf_net_inflow_yuan;
    summary["etf_share_ranking_nonzero_net_inflow"] =
        etf_nonzero_net_inflow;
    summary["configured_empty_sources"] = configured_empty_sources;

    Json result = Json::object();
    result["schema"] = "tdx-market-exchange-funds-native-v1";
    result["generated_at"] = detail::now_text();
    result["view"] = options.view;
    result["mode"] = options.code.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["quote_source"] = std::move(quote_source);
    result["quote_errors"] = std::move(quote_errors);
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    cache["quote_refreshed"] = quote_refreshed;
    cache["quote_age_seconds"] = quote_age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
