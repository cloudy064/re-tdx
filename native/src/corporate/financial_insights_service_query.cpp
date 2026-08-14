#include "financial_insights_internal.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace tdx {

Json FinancialInsightsService::query(const FinancialInsightsQuery& options) {
    if (options.view != "all" &&
        !detail::financial_insights::find_view(options.view))
        throw Error("unsupported financial-insights view");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const auto selected_market = lower_ascii(trim(options.market));
    if (!selected_market.empty() && selected_market != "sz" &&
        selected_market != "sh" && selected_market != "bj")
        throw Error("market must be sz/sh/bj");
    if (!options.code.empty() && !detail::financial_insights::digits(options.code))
        throw Error("code must contain six digits");
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");
    const auto selected_sort = detail::financial_insights::sort_field(options.sort);
    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && detail::financial_insights::text_value(row, "kind") != options.view) continue;
        const auto& security = row.at("security");
        if (!selected_market.empty() &&
            (security.at("market").as_string() != selected_market ||
             security.at("code").as_string() != options.code)) continue;
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
            continue;
        records.push_back(row);
    }
    const bool ascending = options.order == "asc";
    std::sort(records.as_array().begin(), records.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto lid = left.at("record_id").as_string();
            const auto rid = right.at("record_id").as_string();
            if (selected_sort.empty()) return ascending ? lid < rid : lid > rid;
            const auto lv = detail::financial_insights::normalized_number(left, selected_sort);
            const auto rv = detail::financial_insights::normalized_number(right, selected_sort);
            if (lv && rv && *lv != *rv) return ascending ? *lv < *rv : *lv > *rv;
            if (lv.has_value() != rv.has_value()) return lv.has_value();
            return lid < rid;
        });
    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    for (const auto& row : records.as_array()) {
        ++counts[detail::financial_insights::text_value(row, "kind")];
        securities.insert(row.at("security").at("security_id").as_string());
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    if (!options.include_raw)
        for (auto& row : records.as_array()) row.as_object().erase("raw");
    Json summary = Json::object();
    for (const auto& definition :
         detail::financial_insights::resource_definitions())
        summary[std::string(definition.kind)] =
            counts[std::string(definition.kind)];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    Json result = Json::object();
    result["schema"] = "tdx-market-financial-insights-native-v1";
    result["generated_at"] =
        detail::financial_insights::current_time_text();
    result["view"] = options.view;
    result["sort"] = options.sort;
    result["order"] = options.order;
    result["mode"] = selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "Fifteen client-curated financial screens with CFG-calibrated units and formulas. "
        "Ten-thousand-yuan/share and hundred-million-yuan fields expose base-unit companions. "
        "Profit-warning actual profit is the client-provided smaller value of net profit and "
        "profit excluding non-recurring items. Earnings-reversal growth uses the absolute "
        "disclosed-period base. The quality-growth criteria reproduce the four monotonic CFG "
        "conditions, and dividend per-ten-share values expose per-share companions. Host-only "
        "live price, three-year return, dividend yield and other non-JSN columns are not fabricated.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
