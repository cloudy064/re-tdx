#include "strong_stocks_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>

namespace tdx::detail::strong_stocks {

Json interval_summary(const Json& rows) {
    std::set<std::string> securities;
    std::uint64_t names = 0, finalized = 0, repeated = 0;
    std::uint64_t market_sz = 0, market_sh = 0, market_bj = 0;
    std::map<std::string, std::uint64_t> occurrences;
    std::string first_start, latest_start, latest_end;
    for (const auto& row : rows.as_array()) {
        const auto& security = row.at("security");
        const auto id = security.at("security_id").as_string();
        securities.insert(id);
        ++occurrences[id];
        if (security.at("name_resolved").as_bool()) ++names;
        const auto market = security.at("market").as_string();
        if (market == "sz") ++market_sz;
        else if (market == "sh") ++market_sh;
        else if (market == "bj") ++market_bj;
        if (row.at("return_finalized").as_bool()) ++finalized;
        const auto start = json_text(row, "start_date");
        const auto end = json_text(row, "end_date");
        if (first_start.empty() || (!start.empty() && start < first_start)) first_start = start;
        latest_start = std::max(latest_start, start);
        latest_end = std::max(latest_end, end);
    }
    for (const auto& [id, count] : occurrences) {
        (void)id;
        if (count > 1) ++repeated;
    }
    Json result = Json::object();
    result["source_rows"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["securities_with_multiple_intervals"] = repeated;
    result["names_resolved"] = names;
    result["return_finalized_rows"] = finalized;
    result["return_pending_rows"] = static_cast<std::uint64_t>(rows.size()) - finalized;
    result["first_start_date"] = first_start.empty() ? Json(nullptr) : Json(first_start);
    result["latest_start_date"] = latest_start.empty() ? Json(nullptr) : Json(latest_start);
    result["latest_end_date"] = latest_end.empty() ? Json(nullptr) : Json(latest_end);
    Json markets = Json::object();
    markets["sz"] = market_sz;
    markets["sh"] = market_sh;
    markets["bj"] = market_bj;
    result["by_market"] = std::move(markets);
    return result;
}

Json detail_summary(const Json& rows, const Json& interval) {
    double amount = 0.0, seal_sum = 0.0;
    std::uint64_t amount_count = 0, positive_days = 0, reason_days = 0,
                  seal_count = 0, limit_up_sum = 0, broken_sum = 0,
                  limit_down_sum = 0;
    std::string first_date, latest_date;
    for (const auto& row : rows.as_array()) {
        const auto date = json_text(row, "date");
        if (first_date.empty() || (!date.empty() && date < first_date)) first_date = date;
        latest_date = std::max(latest_date, date);
        const auto stock_return = json_number(row, "stock_return_pct");
        if (stock_return && *stock_return > 0) ++positive_days;
        if (!json_text(row, "limit_up_reason").empty()) ++reason_days;
        const auto turnover = json_number(row, "turnover_amount_yuan");
        if (turnover) { amount += *turnover; ++amount_count; }
        const auto limit_up = json_number(row, "market_limit_up_count");
        const auto broken = json_number(row, "market_broken_limit_count");
        const auto limit_down = json_number(row, "market_limit_down_count");
        if (limit_up) limit_up_sum += static_cast<std::uint64_t>(*limit_up);
        if (broken) broken_sum += static_cast<std::uint64_t>(*broken);
        if (limit_down) limit_down_sum += static_cast<std::uint64_t>(*limit_down);
        const auto seal = json_number(row, "market_seal_success_pct");
        if (seal) { seal_sum += *seal; ++seal_count; }
    }
    const auto expected = json_number(interval, "trading_days");
    Json result = Json::object();
    result["days"] = static_cast<std::uint64_t>(rows.size());
    result["expected_trading_days"] = expected ? Json(*expected) : Json(nullptr);
    result["complete"] = expected && static_cast<std::uint64_t>(*expected) == rows.size();
    result["first_date"] = first_date.empty() ? Json(nullptr) : Json(first_date);
    result["latest_date"] = latest_date.empty() ? Json(nullptr) : Json(latest_date);
    result["positive_stock_days"] = positive_days;
    result["reason_days"] = reason_days;
    result["turnover_amount_yuan"] = amount_count ? Json(amount) : Json(nullptr);
    result["market_limit_up_count_sum"] = limit_up_sum;
    result["market_broken_limit_count_sum"] = broken_sum;
    result["market_limit_down_count_sum"] = limit_down_sum;
    result["average_market_seal_success_pct"] = seal_count
        ? Json(seal_sum / static_cast<double>(seal_count)) : Json(nullptr);
    return result;
}

}  // namespace tdx::detail::strong_stocks

