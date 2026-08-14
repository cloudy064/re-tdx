#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace tdx {

using namespace convertible_bond_detail;

Json ConvertibleBondService::query_listed(
    const ConvertibleBondQuery& options, int selected_market) {
    bool refreshed = false;
    int age_seconds = 0;
    const std::string view = "listed";
    const auto master = fetch_master(options, refreshed, age_seconds);
    Json rows = Json::array();
    const auto folded = lower_ascii(trim(options.query));
    for (const auto& row : master.at("rows").as_array()) {
        const auto& bond = row.at("bond");
        const auto& underlying = row.at("underlying");
        bool matches = options.code.empty();
        if (!options.code.empty()) {
            matches = static_cast<int>(bond.at("market_id").as_number()) == selected_market &&
                      bond.at("code").as_string() == options.code;
            if (!matches && !underlying.is_null())
                matches = static_cast<int>(underlying.at("market_id").as_number()) == selected_market &&
                          underlying.at("code").as_string() == options.code;
        }
        if (matches && !folded.empty()) {
            const auto haystack = lower_ascii(bond.at("security_id").as_string() + " " +
                bond.at("name").as_string() + " " +
                (underlying.is_null() ? "" : underlying.at("security_id").as_string() +
                    " " + underlying.at("name").as_string()));
            matches = haystack.find(folded) != std::string::npos;
        }
        if (matches) rows.push_back(row);
    }
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(), [](const Json& left, const Json& right) {
        const auto l = value_number(left.at("overview"), "remaining_balance_100m_yuan").value_or(0.0);
        const auto r = value_number(right.at("overview"), "remaining_balance_100m_yuan").value_or(0.0);
        return l > r;
    });
    const auto matched = rows.size();
    while (static_cast<int>(rows.size()) > options.limit) rows.as_array().pop_back();
    Json details = Json::array();
    Json detail_errors = Json::array();
    if (!options.code.empty() && options.include_details) {
        for (const auto& bond : rows.as_array()) {
            const auto& security = bond.at("bond");
            const auto dynamic_key = std::to_string(static_cast<int>(security.at("market_id").as_number())) +
                                     security.at("code").as_string() + ".jsn";
            const std::array<std::pair<std::string, std::string>, 3> resources{{
                {"sellback", "kzz_hstk/" + dynamic_key},
                {"redemption", "kzz_shtk/" + dynamic_key},
                {"revision", "kzz_xztk/" + dynamic_key}}};
            Json bond_detail = Json::object();
            bond_detail["bond"] = security;
            for (const auto& [kind, resource] : resources) {
                try {
                    const auto source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
                    bond_detail[kind] = normalize_detail(source, kind);
                } catch (const std::exception& error) {
                    bond_detail[kind] = Json::array();
                    Json failure = Json::object();
                    failure["bond"] = security.at("security_id");
                    failure["resource"] = resource;
                    failure["message"] = error.what();
                    detail_errors.push_back(std::move(failure));
                }
            }
            details.push_back(std::move(bond_detail));
        }
    }
    Json summary = Json::object();
    summary["bonds"] = static_cast<std::uint64_t>(master.at("rows").size());
    std::uint64_t sellback_triggered = 0, redemption_triggered = 0, revisions = 0;
    std::uint64_t exchangeable = 0, exchangeable_supplemented = 0,
        exchangeable_projection_verified = 0, complete_overviews = 0;
    double remaining = 0;
    for (const auto& row : master.at("rows").as_array()) {
        remaining += value_number(row.at("overview"), "remaining_balance_100m_yuan").value_or(0.0);
        if (value_number(row.at("sellback"), "history_count").value_or(0) > 0) ++sellback_triggered;
        if (value_number(row.at("redemption"), "history_count").value_or(0) > 0) ++redemption_triggered;
        revisions += static_cast<std::uint64_t>(std::max(0.0,
            value_number(row.at("revision"), "history_count").value_or(0.0)));
        if (row.at("instrument_type").as_string() == "exchangeable-bond") ++exchangeable;
        if (row.at("exchangeable_supplemented").as_bool()) ++exchangeable_supplemented;
        if (row.at("exchangeable_projection_verified").as_bool())
            ++exchangeable_projection_verified;
        if (row.at("overview").at("core_terms_complete").as_bool()) ++complete_overviews;
    }
    summary["remaining_balance_100m_yuan"] = remaining;
    summary["sellback_triggered_bonds"] = sellback_triggered;
    summary["redemption_triggered_bonds"] = redemption_triggered;
    summary["conversion_price_revisions"] = revisions;
    summary["exchangeable_bonds"] = exchangeable;
    summary["exchangeable_bonds_supplemented"] = exchangeable_supplemented;
    summary["exchangeable_bonds_projection_verified"] =
        exchangeable_projection_verified;
    summary["core_terms_complete"] = complete_overviews;
    summary["core_terms_missing"] = static_cast<std::uint64_t>(master.at("rows").size()) -
                                      complete_overviews;
    Json result = Json::object();
    result["schema"] = "tdx-market-convertible-bonds-native-v1";
    result["generated_at"] = now_text();
    result["view"] = view;
    result["mode"] = options.code.empty() ? "catalog" : "security";
    result["availability"] = jsn_sources_health(master.at("sources")).at("stale").as_bool()
        ? "stale-cache" : master.at("supplement_errors").size() ? "partial" : "live";
    result["query"] = options.query;
    result["found"] = options.code.empty() || matched > 0;
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["bonds"] = std::move(rows);
    result["pricing"] = Json::array();
    result["pending_issues"] = Json::array();
    result["subscriptions"] = Json::array();
    result["exchangeable_projection_reconciliation"] =
        master.at("exchangeable_projection_reconciliation");
    result["details"] = std::move(details);
    result["detail_errors"] = std::move(detail_errors);
    result["master_errors"] = master.at("supplement_errors");
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["upstream_health"] = jsn_sources_health(master.at("sources"));
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx

