#include "reverse_repo_internal.hpp"

#include "tdx/jsn.hpp"

namespace tdx {
Json ReverseRepoService::query(const ReverseRepoQuery& input) {
    using namespace detail::reverse_repo;
    const auto plan = make_query_plan(input);
    Json sources = Json::array(), warnings = Json::array(), quote_source = Json(nullptr);
    Json records = Json::array();
    bool schedule_refreshed = false, quote_refreshed = false;
    if (plan.view->kind == ViewKind::catalog) {
        records = catalog_rows();
    } else {
        const auto schedule = fetch_schedule(plan.options.refresh,
            plan.options.cache_ttl_seconds, plan.options.timeout_ms, schedule_refreshed);
        sources.push_back(jsn_source_metadata(schedule));
        Json quote_rows = Json::array();
        if (plan.options.include_quotes) {
            std::vector<std::string> securities;
            for (const auto& raw : schedule.at("rows").as_array()) {
                const auto code = text_value(raw, "$ZQDM");
                if (!digits(code, 6)) continue;
                try { const auto id = market_id(text_value(raw, "$SC"));
                    securities.push_back(market_name(id) + ":" + code); }
                catch (...) {}
            }
            try {
                const auto quotes = fetch_quotes(securities, plan.options.refresh,
                    plan.options.quote_cache_ttl_seconds, plan.options.timeout_ms,
                    quote_refreshed);
                quote_rows = quotes.at("records");
                quote_source = Json::object();
                for (const auto* key : {"command", "endpoint", "server_name",
                                        "generated_at", "requested", "received",
                                        "transport"})
                    quote_source[key] = quotes.at(key);
                quote_source["cache_refreshed"] = quote_refreshed;
            } catch (const std::exception& error) {
                Json warning = Json::object(); warning["source"] = "public-l1-snapshot";
                warning["message"] = error.what(); warnings.push_back(std::move(warning));
            }
        }
        records = normalize_reverse_repo_rows(schedule.at("rows"), quote_rows,
            plan.options.principal_yuan, names_.securities);
    }

    Json filtered = Json::array(); const auto needle = lower_ascii(plan.options.query);
    for (const auto& row : records.as_array()) {
        if (plan.view->kind == ViewKind::catalog) {
            if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
            continue;
        }
        const auto& security = row.at("security");
        if (plan.selected_market >= 0 &&
            static_cast<int>(security.at("market_id").as_number()) != plan.selected_market) continue;
        if (!plan.options.code.empty() && security.at("code").as_string() != plan.options.code) continue;
        const auto term = static_cast<int>(row.at("term_days").as_number());
        if (plan.options.min_term_days && term < plan.options.min_term_days) continue;
        if (plan.options.max_term_days && term > plan.options.max_term_days) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    if (plan.sort) sort_rows(filtered, *plan.sort, plan.options.order);
    const auto summary = plan.view->kind == ViewKind::catalog
        ? Json::object() : summary_for(filtered, plan.options.principal_yuan);
    const auto matched = filtered.size(); Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(plan.options.offset);
         index < filtered.size() && paged.size() < static_cast<std::size_t>(plan.options.limit); ++index)
        paged.push_back(filtered.as_array()[index]);
    const auto health = jsn_sources_health(sources);
    const auto quoted = plan.view->kind == ViewKind::catalog ? 0 :
        static_cast<int>(summary.at("quoted_rows").as_number());
    Json result = Json::object(); result["schema"] = "tdx-market-reverse-repo-native-v1";
    result["generated_at"] = now_text(); result["view"] = plan.options.view;
    result["availability"] = plan.view->kind == ViewKind::catalog ? "catalog" :
        health.at("stale").as_bool() ? "stale-cache" : matched == 0 ? "empty" :
        quoted == 0 ? "schedule-only" : "live";
    Json filters = Json::object(); filters["market"] = plan.options.market;
    filters["code"] = plan.options.code.empty() ? Json(nullptr) : Json(plan.options.code);
    filters["query"] = plan.options.query; filters["min_term_days"] = plan.options.min_term_days;
    filters["max_term_days"] = plan.options.max_term_days; filters["principal_yuan"] = plan.options.principal_yuan;
    filters["include_quotes"] = plan.options.include_quotes; filters["sort"] = plan.options.sort;
    filters["order"] = plan.options.order; result["filters"] = std::move(filters);
    result["summary"] = summary; Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched); counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size()); counts["warnings"] = static_cast<std::uint64_t>(warnings.size());
    result["counts"] = std::move(counts); result["records"] = std::move(paged);
    result["catalog"] = catalog_rows(); result["sources"] = std::move(sources);
    result["quote_source"] = std::move(quote_source); result["upstream_health"] = health;
    result["warnings"] = std::move(warnings); Json cache = Json::object();
    cache["schedule_ttl_seconds"] = plan.options.cache_ttl_seconds;
    cache["quote_ttl_seconds"] = plan.options.quote_cache_ttl_seconds;
    cache["schedule_refreshed"] = schedule_refreshed; cache["quote_refreshed"] = quote_refreshed;
    result["cache"] = std::move(cache); Json units = Json::object();
    units["annualized_rate_pct"] = "percent-per-year"; units["net_annualized_rate_pct"] = "percent-per-year-after-fee";
    units["gross_interest_yuan"] = "yuan-for-selected-principal"; units["fee_yuan"] = "yuan-for-selected-principal";
    units["net_interest_yuan"] = "yuan-for-selected-principal"; units["turnover_amount_yuan"] = "yuan";
    result["units"] = std::move(units);
    result["semantics"] = "TDX GZNHG reverse-repo calendar joined with the public 0x054C L1 snapshot. TS is contractual term days, SJTS is actual interest-bearing days, SXF is fee yuan per CNY 100,000, SCZJJS is first settlement date, ZJKY is funds-available date and ZJKQ is funds-withdrawable date. The client formula gross interest = principal * annualized_rate/100 * interest_days/365 is reproduced; fee and transparent net interest/net annualized rate are derived for principal_yuan. GC/R repo quote prices are annualized percent rates and require the repo-specific two-decimal scale; 1.300% is not 130%. During market closure the public snapshot is the latest available quote, not a promise of a new trade. Quote failure degrades to schedule-only instead of hiding the settlement calendar. This public L1 feature does not require L2.";
    return result;
}
}  // namespace tdx
