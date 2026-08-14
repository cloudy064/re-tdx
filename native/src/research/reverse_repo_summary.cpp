#include "reverse_repo_internal.hpp"

#include <algorithm>

namespace tdx::detail::reverse_repo {
Json summary_for(const Json& rows, int principal_yuan) {
    std::uint64_t quoted = 0, resolved = 0, sz = 0, sh = 0;
    int minimum_term = 0, maximum_term = 0;
    std::string schedule_date;
    const Json* best_rate = nullptr;
    const Json* best_net_rate = nullptr;
    for (const auto& row : rows.as_array()) {
        const auto term = static_cast<int>(row.at("term_days").as_number());
        minimum_term = minimum_term == 0 ? term : std::min(minimum_term, term);
        maximum_term = std::max(maximum_term, term);
        schedule_date = std::max(schedule_date, row.at("settlement_date").as_string());
        if (row.at("quote_available").as_bool()) ++quoted;
        if (row.at("security").at("name_resolved").as_bool()) ++resolved;
        if (row.at("security").at("market").as_string() == "sz") ++sz;
        else if (row.at("security").at("market").as_string() == "sh") ++sh;
        const auto rate = json_number(row, "annualized_rate_pct");
        const auto net_rate = json_number(row, "net_annualized_rate_pct");
        if (rate && (!best_rate || *rate > *json_number(*best_rate, "annualized_rate_pct"))) best_rate = &row;
        if (net_rate && (!best_net_rate || *net_rate > *json_number(*best_net_rate, "net_annualized_rate_pct"))) best_net_rate = &row;
    }
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size()); result["quoted_rows"] = quoted;
    result["names_resolved"] = resolved; result["principal_yuan"] = principal_yuan;
    result["minimum_term_days"] = minimum_term; result["maximum_term_days"] = maximum_term;
    result["schedule_date"] = schedule_date.empty() ? Json(nullptr) : Json(schedule_date);
    Json markets = Json::object(); markets["sz"] = sz; markets["sh"] = sh; result["by_market"] = std::move(markets);
    result["best_rate"] = best_rate ? Json::object() : Json(nullptr);
    if (best_rate) { result["best_rate"]["security"] = best_rate->at("security"); result["best_rate"]["annualized_rate_pct"] = best_rate->at("annualized_rate_pct"); }
    result["best_net_rate"] = best_net_rate ? Json::object() : Json(nullptr);
    if (best_net_rate) { result["best_net_rate"]["security"] = best_net_rate->at("security"); result["best_net_rate"]["net_annualized_rate_pct"] = best_net_rate->at("net_annualized_rate_pct"); result["best_net_rate"]["net_interest_yuan"] = best_net_rate->at("net_interest_yuan"); }
    return result;
}
}  // namespace tdx::detail::reverse_repo
