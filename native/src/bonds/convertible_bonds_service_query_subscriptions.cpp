#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cstdint>

namespace tdx {

using namespace convertible_bond_detail;

Json ConvertibleBondService::query_subscriptions(
    const ConvertibleBondQuery& options, int selected_market) {
    bool refreshed = false;
    int age_seconds = 0;
    const std::string view = "subscriptions";
        const auto master = fetch_subscriptions(options, refreshed, age_seconds);
        Json rows = Json::array();
        Json projection_rows = Json::array();
        const auto folded = lower_ascii(trim(options.query));
        for (const auto& row : master.at("rows").as_array()) {
            const auto& bond = row.at("bond");
            const auto& underlying = row.at("underlying");
            bool matches = options.code.empty() ||
                (static_cast<int>(bond.at("market_id").as_number()) == selected_market &&
                 bond.at("code").as_string() == options.code) ||
                (static_cast<int>(underlying.at("market_id").as_number()) == selected_market &&
                 underlying.at("code").as_string() == options.code);
            if (matches && !folded.empty()) {
                const auto haystack = lower_ascii(
                    bond.at("security_id").as_string() + " " +
                    bond.at("name").as_string() + " " +
                    underlying.at("security_id").as_string() + " " +
                    underlying.at("name").as_string() + " " +
                    row.at("subscription_code").as_string());
                matches = haystack.find(folded) != std::string::npos;
            }
            if (matches) rows.push_back(row);
        }
        for (const auto& row : master.at("new_bond_projection").as_array()) {
            const auto& underlying = row.at("underlying");
            bool matches = options.code.empty() ||
                (static_cast<int>(underlying.at("market_id").as_number()) ==
                    selected_market &&
                 underlying.at("code").as_string() == options.code);
            if (matches && !folded.empty()) {
                const auto haystack = lower_ascii(
                    underlying.at("security_id").as_string() + " " +
                    underlying.at("name").as_string() + " " +
                    row.at("subscription_code").as_string() + " " +
                    row.at("subscription_name").as_string() + " " +
                    row.at("plan_progress").as_string());
                matches = haystack.find(folded) != std::string::npos;
            }
            if (matches) projection_rows.push_back(row);
        }
        std::stable_sort(projection_rows.as_array().begin(),
                         projection_rows.as_array().end(),
            [](const Json& left, const Json& right) {
                return left.at("subscription_date").as_string() >
                    right.at("subscription_date").as_string();
            });
        sort_convertible_bond_subscription_rows(rows, options.sort, options.order);
        const auto matched = rows.size();
        const auto projection_matched = projection_rows.size();
        while (static_cast<int>(rows.size()) > options.limit) rows.as_array().pop_back();
        while (static_cast<int>(projection_rows.size()) > options.limit)
            projection_rows.as_array().pop_back();
        std::uint64_t listed = 0, formula_complete = 0;
        double issue_size = 0.0;
        for (const auto& row : master.at("rows").as_array()) {
            if (row.at("listed").as_bool()) ++listed;
            if (row.at("conversion_value_yuan").is_number() &&
                row.at("conversion_premium_pct").is_number()) ++formula_complete;
            if (row.at("issue_size_100m_yuan").is_number())
                issue_size += row.at("issue_size_100m_yuan").as_number();
        }
        Json summary = Json::object();
        summary["subscriptions"] = static_cast<std::uint64_t>(master.at("rows").size());
        summary["listed"] = listed;
        summary["not_listed"] = static_cast<std::uint64_t>(master.at("rows").size()) - listed;
        summary["formula_complete"] = formula_complete;
        summary["issue_size_100m_yuan"] = issue_size;
        summary["new_bond_projection_rows"] = static_cast<std::uint64_t>(
            master.at("new_bond_projection").size());
        if (master.at("new_bond_reconciliation").is_object() &&
            master.at("new_bond_reconciliation").as_object().count(
                "hybrid_or_stale_count"))
            summary["new_bond_hybrid_or_stale"] =
                master.at("new_bond_reconciliation").at("hybrid_or_stale_count");
        else summary["new_bond_hybrid_or_stale"] = Json(nullptr);
        const auto sources = master.at("sources");
        const auto health = jsn_sources_health(sources);
        Json result = Json::object();
        result["schema"] = "tdx-market-convertible-bonds-native-v1";
        result["generated_at"] = now_text();
        result["view"] = view;
        result["mode"] = options.code.empty() ? "catalog" : "security";
        result["availability"] = health.at("stale").as_bool()
            ? "stale-cache" : master.at("projection_errors").size()
                ? "partial" : "live";
        result["query"] = options.query;
        result["sort"] = options.sort.empty() ? "subscription-date" : lower_ascii(trim(options.sort));
        result["order"] = lower_ascii(trim(options.order.empty() ? "desc" : options.order));
        result["found"] = options.code.empty() || matched > 0 ||
            projection_matched > 0;
        result["match_count"] = static_cast<std::uint64_t>(matched);
        result["new_bond_projection_match_count"] =
            static_cast<std::uint64_t>(projection_matched);
        result["returned"] = static_cast<std::uint64_t>(rows.size());
        result["bonds"] = Json::array();
        result["pricing"] = Json::array();
        result["pending_issues"] = Json::array();
        result["subscriptions"] = std::move(rows);
        result["new_bond_projection"] = std::move(projection_rows);
        result["new_bond_reconciliation"] =
            master.at("new_bond_reconciliation");
        result["details"] = Json::array();
        result["detail_errors"] = Json::array();
        result["master_errors"] = master.at("projection_errors");
        result["summary"] = std::move(summary);
        result["sources"] = sources;
        result["upstream_health"] = health;
        Json cache = Json::object();
        cache["refreshed"] = refreshed;
        cache["age_seconds"] = age_seconds;
        cache["ttl_seconds"] = options.cache_ttl_seconds;
        result["cache"] = std::move(cache);
        return result;
    
}

}  // namespace tdx
