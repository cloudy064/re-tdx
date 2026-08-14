#include "ownership_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace tdx {

using namespace ownership_detail;
Json OwnershipService::query(const OwnershipQuery& options) {
    const std::set<std::string> views{
        "changes", "plans", "insiders", "commitments", "pledges",
        "statistics", "institutions", "rankings", "shareholder-counts"};
    if (!views.count(options.view))
        throw Error("view must be changes, plans, insiders, commitments, pledges, "
                    "statistics, institutions, rankings, or shareholder-counts");
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("detail_limit must be in 1..5000");
    if (options.master_cache_ttl_seconds < 0 ||
        options.detail_cache_ttl_seconds < 0)
        throw Error("cache TTL must be non-negative");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const bool security_mode = !options.code.empty();
    const int selected_market = security_mode ? market_id(options.market) : -1;
    if (security_mode && !digits(options.code, 6))
        throw Error("code must contain six digits");
    const bool institution_mode = !options.institution_id.empty();
    if (institution_mode && !safe_identifier(options.institution_id))
        throw Error("institution_id contains unsupported characters");
    if (institution_mode && options.view != "institutions")
        throw Error("institution_id requires view=institutions");

    static const std::map<std::string, std::set<std::string>> categories{
        {"changes", {"all", "increase", "decrease"}},
        {"plans", {"all", "increase", "decrease"}},
        {"insiders", {"all", "increase", "decrease"}},
        {"commitments", {"all", "active", "upcoming", "expired"}},
        {"pledges", {"all", "latest", "warning", "liquidation", "release"}},
        {"statistics", {"all", "changes", "pledges"}},
        {"institutions", {"all", "trust", "broker"}},
        {"rankings", {"all", "increase-ratio", "increase-value",
                      "increase-count", "decrease-ratio", "decrease-value",
                      "decrease-count"}},
        {"shareholder-counts", {"all", "sh-main", "sz-main",
                                "sz-sme-legacy", "chinext", "star", "bj"}},
    };
    const auto allowed = categories.find(options.view);
    if (!allowed->second.count(options.category))
        throw Error("unsupported category for the selected ownership view");

    auto core = options.view == "rankings"
        ? fetch_rankings(options)
        : options.view == "shareholder-counts"
            ? fetch_shareholder_counts(options) : fetch_core(options);
    Json changes = Json::array(), plans = Json::array(), insiders = Json::array();
    Json commitments = Json::array(), pledges = Json::array();
    Json change_history = Json::array(), insider_history = Json::array();
    Json pledge_history = Json::array();
    Json change_monthly = Json::array(), change_annual = Json::array(),
         change_count_trend = Json::array();
    Json pledge_monthly = Json::array(), institutions = Json::array();
    Json rankings = Json::array();
    Json shareholder_counts = Json::array();
    Json institution_details = Json::array(), errors = Json::array();
    Json extra_sources = Json::array();
    bool detail_refreshed = false;
    int detail_age = 0;

    if (options.view == "changes") {
        Json candidates = Json::array();
        if (options.category == "all" || options.category == "increase")
            append_rows(candidates, core.document.at("change_increase"));
        if (options.category == "all" || options.category == "decrease")
            append_rows(candidates, core.document.at("change_decrease"));
        std::stable_sort(candidates.as_array().begin(), candidates.as_array().end(),
            [](const Json& left, const Json& right) {
                return text_value(left, "announcement_date") >
                       text_value(right, "announcement_date");
            });
        changes = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }
    if (options.view == "plans") {
        Json candidates = Json::array();
        if (options.category == "all" || options.category == "increase")
            append_rows(candidates, core.document.at("plan_increase"));
        if (options.category == "all" || options.category == "decrease")
            append_rows(candidates, core.document.at("plan_decrease"));
        std::stable_sort(candidates.as_array().begin(), candidates.as_array().end(),
            [](const Json& left, const Json& right) {
                return text_value(left, "announcement_date") >
                       text_value(right, "announcement_date");
            });
        plans = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }
    if (options.view == "insiders") {
        Json candidates = Json::array();
        for (const auto& row : core.document.at("insiders").as_array()) {
            if (options.category != "all" &&
                text_value(row, "direction") != options.category)
                continue;
            candidates.push_back(row);
        }
        insiders = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }
    if (options.view == "commitments") {
        Json candidates = Json::array();
        for (const auto& row : core.document.at("commitments").as_array()) {
            if (options.category != "all" &&
                text_value(row, "status") != options.category)
                continue;
            candidates.push_back(row);
        }
        commitments = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }
    if (options.view == "pledges") {
        Json candidates = Json::array();
        if (options.category == "all" || options.category == "latest")
            append_rows(candidates, core.document.at("pledge_latest"));
        if (options.category == "all" || options.category == "warning")
            append_rows(candidates, core.document.at("pledge_warning"));
        if (options.category == "all" || options.category == "liquidation")
            append_rows(candidates, core.document.at("pledge_liquidation"));
        if (options.category == "all" || options.category == "release")
            append_rows(candidates, core.document.at("pledge_release"));
        pledges = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }
    if (options.view == "statistics") {
        if (options.category == "all" || options.category == "changes") {
            change_monthly = core.document.at("change_monthly");
            change_annual = core.document.at("change_annual");
        }
        if (options.category == "all" || options.category == "pledges")
            pledge_monthly = core.document.at("pledge_monthly");
    }
    if (options.view == "institutions") {
        Json candidates = Json::array();
        if (options.category == "all" || options.category == "trust")
            append_rows(candidates, core.document.at("institution_trust"));
        if (options.category == "all" || options.category == "broker")
            append_rows(candidates, core.document.at("institution_broker"));
        institutions = limited_filtered(candidates, options.query, options.limit);
    }
    if (options.view == "rankings") {
        Json candidates = Json::array();
        const std::vector<std::pair<std::string, std::string>> groups{
            {"increase-ratio", "increase_ratio"},
            {"increase-value", "increase_value"},
            {"increase-count", "increase_count"},
            {"decrease-ratio", "decrease_ratio"},
            {"decrease-value", "decrease_value"},
            {"decrease-count", "decrease_count"}};
        for (const auto& [category, key] : groups)
            if (options.category == "all" || options.category == category)
                append_rows(candidates, core.document.at(key));
        rankings = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }
    if (options.view == "shareholder-counts") {
        Json candidates = Json::array();
        const std::vector<std::pair<std::string, std::string>> groups{
            {"sh-main", "sh_main"}, {"sz-main", "sz_main"},
            {"sz-sme-legacy", "sz_sme_legacy"}, {"chinext", "chinext"},
            {"star", "star"}, {"bj", "bj"}};
        for (const auto& [category, key] : groups)
            if (options.category == "all" || options.category == category)
                append_rows(candidates, core.document.at(key));
        shareholder_counts = limited_filtered(select_security(
            candidates, security_mode, selected_market, options.code),
            options.query, options.limit);
    }

    auto fetch_detail = [&](const std::string& resource, auto normalizer,
                            Json& destination) {
        try {
            auto fetched = fetch_resource(resource, options);
            extra_sources.push_back(source_summary(fetched.document));
            detail_refreshed = detail_refreshed || fetched.refreshed;
            detail_age = std::max(detail_age, fetched.age_seconds);
            destination = normalizer(fetched.document.at("rows"));
            if (static_cast<int>(destination.size()) > options.detail_limit)
                destination.as_array().resize(
                    static_cast<std::size_t>(options.detail_limit));
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = resource;
            failure["message"] = error.what();
            errors.push_back(std::move(failure));
        }
    };
    if (security_mode && options.include_details &&
        (options.view == "changes" || options.view == "plans")) {
        const auto resource = "zcjc/" + std::to_string(selected_market) +
                              options.code + ".jsn";
        fetch_detail(resource, [&](const Json& rows) {
            return normalize_ownership_change_rows(rows, "auto", securities_, false);
        }, change_history);
    }
    if (security_mode && options.include_details && options.view == "pledges") {
        const auto resource = "gqzy/" + std::to_string(selected_market) +
                              options.code + ".jsn";
        fetch_detail(resource, [&](const Json& rows) {
            return normalize_pledge_history_rows(rows);
        }, pledge_history);
    }
    if (security_mode && options.include_details && options.view == "insiders") {
        const auto resource = "cggg/" + std::to_string(selected_market) +
                              options.code + ".jsn";
        fetch_detail(resource, [&](const Json& rows) {
            return normalize_insider_change_rows(rows, securities_, false);
        }, insider_history);
    }
    if (institution_mode && options.include_details) {
        const auto resource = "xtzy/" + options.institution_id + ".jsn";
        fetch_detail(resource, [&](const Json& rows) {
            return normalize_pledge_institution_detail_rows(rows, securities_);
        }, institution_details);
    }
    Json change_count_reconciliation(nullptr);
    if (options.view == "statistics" &&
        (options.category == "all" || options.category == "changes") &&
        !change_monthly.as_array().empty()) {
        const auto period = text_value(change_monthly.as_array().front(), "period");
        fetch_detail("gdzjc1/" + period + ".jsn", [&](const Json& rows) {
            return normalize_ownership_change_count_trend_rows(rows);
        }, change_count_trend);
        std::map<std::string, const Json*> monthly;
        for (const auto& row : change_monthly.as_array())
            monthly[text_value(row, "period")] = &row;
        std::uint64_t matched = 0, mismatch = 0, chart_only = 0;
        for (const auto& row : change_count_trend.as_array()) {
            const auto found = monthly.find(text_value(row, "period"));
            if (found == monthly.end()) { ++chart_only; continue; }
            const auto main_increase = number_value(*found->second, "increase_companies");
            const auto main_decrease = number_value(*found->second, "decrease_companies");
            const auto chart_increase = number_value(row, "increase_companies");
            const auto chart_decrease = number_value(row, "decrease_companies");
            const bool ok = main_increase == chart_increase && main_decrease == chart_decrease;
            ok ? ++matched : ++mismatch;
        }
        change_count_reconciliation = Json::object();
        change_count_reconciliation["master_rows"] = static_cast<std::uint64_t>(change_monthly.size());
        change_count_reconciliation["chart_rows"] = static_cast<std::uint64_t>(change_count_trend.size());
        change_count_reconciliation["matched_rows"] = matched;
        change_count_reconciliation["mismatch_rows"] = mismatch;
        change_count_reconciliation["chart_only_rows"] = chart_only;
        change_count_reconciliation["all_overlaps_match"] = mismatch == 0;
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-ownership-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["category"] = options.category;
    result["mode"] = security_mode ? "security" : institution_mode ? "institution" : "catalog";
    result["changes"] = std::move(changes);
    result["plans"] = std::move(plans);
    result["insiders"] = std::move(insiders);
    result["commitments"] = std::move(commitments);
    result["pledges"] = std::move(pledges);
    result["change_history"] = std::move(change_history);
    result["insider_history"] = std::move(insider_history);
    result["pledge_history"] = std::move(pledge_history);
    result["change_monthly"] = std::move(change_monthly);
    result["change_annual"] = std::move(change_annual);
    result["change_count_trend"] = std::move(change_count_trend);
    result["change_count_reconciliation"] = std::move(change_count_reconciliation);
    result["pledge_monthly"] = std::move(pledge_monthly);
    result["institutions"] = std::move(institutions);
    result["institution_details"] = std::move(institution_details);
    result["rankings"] = std::move(rankings);
    result["shareholder_counts"] = std::move(shareholder_counts);
    result["selected_security"] = security_mode
        ? security_document(selected_market, options.code, securities_) : Json(nullptr);
    result["selected_institution_id"] = options.institution_id;
    result["summary"] = core.document.at("summary");
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["query"] = options.query;
    filters["category"] = options.category;
    result["filters"] = std::move(filters);
    Json sources = core.document.at("sources");
    for (const auto& source : extra_sources.as_array()) sources.push_back(source);
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    const bool shareholder_empty = options.view == "shareholder-counts" &&
        result.at("shareholder_counts").size() == 0;
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" :
        shareholder_empty ? "empty" : "live";
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = core.refreshed;
    cache["master_age_seconds"] = core.age_seconds;
    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    for (const auto* key : {"changes", "plans", "insiders", "commitments",
                            "pledges", "change_history", "insider_history",
                            "pledge_history", "change_monthly", "change_annual",
                            "change_count_trend",
                            "pledge_monthly", "institutions", "institution_details",
                            "rankings", "shareholder_counts", "detail_errors"})
        counts[key] = static_cast<std::uint64_t>(result.at(key).size());
    result["counts"] = std::move(counts);
    return result;
}

}  // namespace tdx