#include "capital_strength_internal.hpp"

#include <set>

namespace tdx::detail::capital_strength {

Json ranking_summary(const Json& rows) {
    std::uint64_t names = 0, positive_ddx = 0, negative_total = 0,
                  negative_main = 0, market_sz = 0, market_sh = 0,
                  market_bj = 0;
    double total_sum = 0, main_sum = 0;
    bool source_sorted = true;
    std::set<std::string> dates;
    std::optional<double> previous;
    for (const auto& row : rows.as_array()) {
        const auto& security = row.at("security");
        if (security.at("name_resolved").as_bool()) ++names;
        const auto market = security.at("market").as_string();
        if (market == "sz") ++market_sz;
        else if (market == "sh") ++market_sh;
        else if (market == "bj") ++market_bj;
        const auto date = json_text(row, "statistics_date");
        if (!date.empty()) dates.insert(date);
        const auto ddx = json_number(row, "ddx_float_share_pct");
        if (ddx) {
            if (*ddx > 0) ++positive_ddx;
            if (previous && *previous < *ddx) source_sorted = false;
            previous = ddx;
        }
        const auto total = json_number(row, "total_net_inflow_yuan");
        if (total) {
            total_sum += *total;
            if (*total < 0) ++negative_total;
        }
        const auto main = json_number(row, "main_net_inflow_yuan");
        if (main) {
            main_sum += *main;
            if (*main < 0) ++negative_main;
        }
    }
    Json result = Json::object();
    result["source_rows"] = static_cast<std::uint64_t>(rows.size());
    result["names_resolved"] = names;
    result["positive_ddx_rows"] = positive_ddx;
    result["negative_total_net_inflow_rows"] = negative_total;
    result["negative_main_net_inflow_rows"] = negative_main;
    result["total_net_inflow_yuan"] = total_sum;
    result["main_net_inflow_yuan"] = main_sum;
    result["source_ddx_descending"] = source_sorted;
    result["statistics_dates"] = Json::array();
    for (const auto& date : dates) result["statistics_dates"].push_back(date);
    Json markets = Json::object();
    markets["sz"] = market_sz;
    markets["sh"] = market_sh;
    markets["bj"] = market_bj;
    result["by_market"] = std::move(markets);
    return result;
}

}  // namespace tdx::detail::capital_strength
