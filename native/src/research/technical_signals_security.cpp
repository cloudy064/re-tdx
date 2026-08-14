#include "technical_signals_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <chrono>
#include <set>
#include <string>

namespace tdx {
using namespace technical_signals_detail;
Json TechnicalSignalsService::query_security(const TechnicalSignalSecurityQuery& input) {
    TechnicalSignalSecurityQuery options = input;
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    const int selected_market = market_id(options.market);
    if (options.code.size() != 6 ||
        !std::all_of(options.code.begin(), options.code.end(), ::isdigit))
        throw Error("security code must contain exactly six digits");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 3600 ||
        options.timeout_ms < 100 || options.timeout_ms > 600000)
        throw Error("technical signal security query limits are invalid");

    Json security = Json::object();
    security["market"] = market_name(selected_market);
    security["market_id"] = selected_market;
    security["code"] = options.code;
    security["security_id"] = market_prefix(selected_market) + options.code;
    const auto known = blocks_.securities.find({selected_market, options.code});
    security["name"] = known == blocks_.securities.end() ? Json("") : Json(known->second.name);
    security["name_resolved"] = known != blocks_.securities.end() && !known->second.name.empty();

    std::set<std::string> memberships;
    for (const auto& member : blocks_.members) {
        if (member.market_id == selected_market && member.code == options.code)
            memberships.insert(member.block_id);
    }

    const auto started = std::chrono::steady_clock::now();
    Json views = Json::array(), all_hits = Json::array();
    std::size_t successful = 0, failed = 0, stale = 0, truncated = 0;
    for (const auto name : technical_signal_views) {
        const std::string view_name(name);
        Json view = Json::object();
        view["view"] = view_name;
        try {
            TechnicalSignalsQuery query_options;
            query_options.view = view_name;
            query_options.direction = "all";
            query_options.board = selected_board(selected_market, options.code);
            query_options.apply_client_filters = options.apply_client_filters;
            query_options.refresh = options.refresh;
            query_options.limit = 10000;
            query_options.cache_ttl_seconds = options.cache_ttl_seconds;
            query_options.timeout_ms = options.timeout_ms;
            const auto document = query(query_options);
            auto hits = technical_signal_hits_for_security(
                document, options.market, options.code, blocks_);
            for (const auto& hit : hits.as_array()) all_hits.push_back(hit);
            const auto availability = text(document, "availability");
            const bool was_truncated = document.at("counts").at("truncated").as_bool();
            view["status"] = was_truncated ? "truncated" : availability;
            view["generated_at"] = text(document, "generated_at");
            view["source_rows"] = document.at("counts").at("upstream_rows");
            view["available_rows"] = document.at("counts").at("after_client_filter");
            view["hit_count"] = static_cast<std::uint64_t>(hits.size());
            view["hits"] = std::move(hits);
            ++successful;
            if (availability != "live") ++stale;
            if (was_truncated) ++truncated;
        } catch (const std::exception& error) {
            view["status"] = "error";
            view["error"] = error.what();
            view["hit_count"] = 0;
            view["hits"] = Json::array();
            ++failed;
        }
        views.push_back(std::move(view));
    }

    const bool complete = failed == 0 && stale == 0 && truncated == 0;
    Json counts = Json::object();
    counts["views_checked"] =
        static_cast<std::uint64_t>(technical_signal_views.size());
    counts["views_successful"] = static_cast<std::uint64_t>(successful);
    counts["views_failed"] = static_cast<std::uint64_t>(failed);
    counts["views_stale"] = static_cast<std::uint64_t>(stale);
    counts["views_truncated"] = static_cast<std::uint64_t>(truncated);
    counts["block_memberships_checked"] = static_cast<std::uint64_t>(memberships.size());
    counts["hits"] = static_cast<std::uint64_t>(all_hits.size());

    Json result = Json::object();
    result["schema"] = "tdx-technical-signal-security-native-v1";
    result["availability"] = failed ? "partial" : (stale ? "stale-cache" :
                                 (truncated ? "truncated" : "live"));
    result["generated_at"] = now_text();
    result["security"] = std::move(security);
    result["board_branch"] = selected_board(selected_market, options.code);
    result["client_filters_applied"] = options.apply_client_filters;
    result["complete"] = complete;
    result["absence_conclusive"] = complete && all_hits.size() == 0;
    result["counts"] = std::move(counts);
    result["hits"] = std::move(all_hits);
    result["views"] = std::move(views);
    result["elapsed_ms"] = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count());
    return result;
}

}  // namespace tdx
