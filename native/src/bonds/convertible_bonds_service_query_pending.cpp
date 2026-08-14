#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <cstdint>
#include <map>

namespace tdx {

using namespace convertible_bond_detail;

Json ConvertibleBondService::query_pending(
    const ConvertibleBondQuery& options, int selected_market) {
    bool refreshed = false;
    int age_seconds = 0;
    const std::string view = "pending";
        const auto master = fetch_pending(options, refreshed, age_seconds);
        Json rows = Json::array();
        const auto folded = lower_ascii(trim(options.query));
        for (const auto& row : master.at("rows").as_array()) {
            const auto& underlying = row.at("underlying");
            bool matches = options.code.empty() ||
                (static_cast<int>(underlying.at("market_id").as_number()) == selected_market &&
                 underlying.at("code").as_string() == options.code);
            if (matches && !folded.empty()) {
                const auto haystack = lower_ascii(
                    underlying.at("security_id").as_string() + " " +
                    underlying.at("name").as_string() + " " +
                    row.at("issue_type").as_string() + " " +
                    row.at("plan_progress").as_string() + " " +
                    row.at("subscription_code").as_string() + " " +
                    row.at("subscription_name").as_string());
                matches = haystack.find(folded) != std::string::npos;
            }
            if (matches) rows.push_back(row);
        }
        sort_pending_convertible_bond_rows(rows, options.sort, options.order);
        const auto matched = rows.size();
        while (static_cast<int>(rows.size()) > options.limit) rows.as_array().pop_back();

        double issue_size = 0.0;
        std::map<std::string, std::pair<std::uint64_t, double>> progress;
        for (const auto& row : master.at("rows").as_array()) {
            const auto size = value_number(row, "planned_issue_size_100m_yuan").value_or(0.0);
            issue_size += size;
            auto& bucket = progress[row.at("plan_progress").as_string()];
            ++bucket.first;
            bucket.second += size;
        }
        Json progress_summary = Json::array();
        for (const auto& [name, values] : progress) {
            Json item = Json::object();
            item["progress"] = name;
            item["count"] = values.first;
            item["planned_issue_size_100m_yuan"] = values.second;
            progress_summary.push_back(std::move(item));
        }
        Json summary = Json::object();
        summary["pending_issues"] = static_cast<std::uint64_t>(master.at("rows").size());
        summary["planned_issue_size_100m_yuan"] = issue_size;
        summary["by_progress"] = std::move(progress_summary);
        summary["projection_count"] = static_cast<std::uint64_t>(
            master.at("projection_reconciliation").size());
        const auto sources = master.at("sources");
        const auto health = jsn_sources_health(sources);
        Json result = Json::object();
        result["schema"] = "tdx-market-convertible-bonds-native-v1";
        result["generated_at"] = now_text();
        result["view"] = view;
        result["mode"] = options.code.empty() ? "catalog" : "security";
        result["availability"] = health.at("stale").as_bool() ? "stale-cache" :
            master.at("projection_errors").size() ? "partial" : "live";
        result["query"] = options.query;
        result["sort"] = options.sort.empty() ? "progress-date" : lower_ascii(trim(options.sort));
        result["order"] = lower_ascii(trim(options.order.empty() ? "desc" : options.order));
        result["found"] = options.code.empty() || matched > 0;
        result["match_count"] = static_cast<std::uint64_t>(matched);
        result["returned"] = static_cast<std::uint64_t>(rows.size());
        result["bonds"] = Json::array();
        result["pricing"] = Json::array();
        result["pending_issues"] = std::move(rows);
        result["subscriptions"] = Json::array();
        result["projection_reconciliation"] =
            master.at("projection_reconciliation");
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
