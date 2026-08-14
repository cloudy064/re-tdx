#include "industry_profile_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <map>

namespace tdx {

Json IndustryProfileService::query(const IndustryProfileQuery& options) {
    if (options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("detail_limit must be in 1..5000");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("industry profile cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    if (security_mode && !options.industry.empty())
        throw Error("choose either industry or security mode");
    if (!options.industry.empty() && !detail::industry_profile::valid_research_industry_code(options.industry))
        throw Error("industry must be a six-digit 881xxx code");
    if (security_mode && !detail::industry_profile::valid_security_code(options.code)) throw Error("code must contain six digits");

    auto master = fetch_master(options);
    Json result = master.document;
    result["generated_at"] = detail::industry_profile::current_time_text();
    result["mode"] = "master";
    result["selected_industry"] = Json(nullptr);
    result["selected_security"] = Json(nullptr);
    result["industry_path"] = Json::array();
    result["holdings_industry"] = Json(nullptr);
    result["holdings_history"] = Json::array();
    result["shareholder_industries"] = Json::array();
    result["shareholder_securities"] = Json::array();
    result["selected_security_profile"] = Json(nullptr);
    result["detail_errors"] = Json::array();
    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;

    if (options.industry.empty() && !security_mode) {
        const auto upstream_health = jsn_sources_health(result.at("sources"));
        const bool stale = upstream_health.at("stale").as_bool();
        result["availability"] = stale ? "stale-cache" : "live";
        cache["stale"] = stale;
        cache["upstream"] = upstream_health;
        result["cache"] = std::move(cache);
        result["counts"]["holding_history_points"] = 0;
        result["counts"]["shareholder_securities"] = 0;
        result["counts"]["detail_errors"] = 0;
        return result;
    }

    int selected_market = 0;
    std::string selected_code = options.industry;
    if (security_mode) {
        selected_market = detail::industry_profile::canonical_market_id(options.market);
        const auto found = security_blocks_.find({selected_market, options.code});
        if (found == security_blocks_.end() || found->second.empty())
            throw Error("security has no local research-industry assignment");
        selected_code = found->second.front();
        result["mode"] = "security";
        result["selected_security"] = detail::industry_profile::make_security_document(
            selected_market, options.code, securities_);
    } else result["mode"] = "industry";
    if (!blocks_.count(selected_code))
        throw Error("research industry is not present in local hierarchy: " + selected_code);

    std::vector<std::string> path_codes;
    for (std::string cursor = selected_code; !cursor.empty(); cursor = parents_.at(cursor))
        path_codes.push_back(cursor);
    std::reverse(path_codes.begin(), path_codes.end());
    for (const auto& code : path_codes) {
        const auto* record = detail::industry_profile::find_industry_record(master.document, code);
        if (record) result["industry_path"].push_back(*record);
    }
    const auto* selected = detail::industry_profile::find_industry_record(master.document, selected_code);
    if (!selected) throw Error("selected industry is missing from master document");
    result["selected_industry"] = *selected;

    std::string holdings_code;
    for (const auto& code : path_codes) {
        const auto* record = detail::industry_profile::find_industry_record(master.document, code);
        if (record && detail::industry_profile::industry_has_data(*record, "has_holdings")) holdings_code = code;
    }
    std::vector<std::string> shareholder_codes;
    auto is_descendant = [&](std::string code, const std::string& ancestor) {
        while (!code.empty()) {
            if (code == ancestor) return true;
            code = parents_.at(code);
        }
        return false;
    };
    if (blocks_.at(selected_code).level == 1) {
        for (const auto& item : master.document.at("industries").as_array()) {
            const auto code = detail::industry_profile::json_text(item, "code");
            if (detail::industry_profile::industry_has_data(item, "has_shareholder_profile") &&
                is_descendant(code, selected_code)) shareholder_codes.push_back(code);
        }
    } else {
        for (auto cursor = selected_code; !cursor.empty(); cursor = parents_.at(cursor)) {
            const auto* record = detail::industry_profile::find_industry_record(master.document, cursor);
            if (record && detail::industry_profile::industry_has_data(*record, "has_shareholder_profile")) {
                shareholder_codes.push_back(cursor);
                break;
            }
        }
    }

    bool any_detail_refreshed = false;
    int greatest_detail_age = 0;
    if (!holdings_code.empty()) {
        const auto* record = detail::industry_profile::find_industry_record(master.document, holdings_code);
        result["holdings_industry"] = record ? *record : Json(nullptr);
        const auto resource = "hycgmx/" + holdings_code + ".jsn";
        try {
            auto fetched = fetch_detail(resource, options);
            any_detail_refreshed = any_detail_refreshed || fetched.refreshed;
            greatest_detail_age = std::max(greatest_detail_age, fetched.age_seconds);
            result["holdings_history"] = normalize_industry_holdings_history_rows(
                fetched.document.at("rows"));
            result["sources"].push_back(
                detail::industry_profile::resource_source_summary(fetched.document));
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = resource;
            failure["message"] = error.what();
            result["detail_errors"].push_back(std::move(failure));
        }
    }

    std::map<std::pair<int, std::string>, Json> security_rows;
    for (const auto& industry_code : shareholder_codes) {
        const auto* record = detail::industry_profile::find_industry_record(master.document, industry_code);
        if (!record) continue;
        result["shareholder_industries"].push_back(*record);
        const auto& profile = record->at("shareholder_profile");
        const auto detail_id = detail::industry_profile::json_text(profile, "detail_id");
        const auto resource = "hygdrs/" + detail_id + ".jsn";
        try {
            auto fetched = fetch_detail(resource, options);
            any_detail_refreshed = any_detail_refreshed || fetched.refreshed;
            greatest_detail_age = std::max(greatest_detail_age, fetched.age_seconds);
            const auto rows = normalize_industry_shareholder_security_rows(
                fetched.document.at("rows"), securities_);
            for (const auto& row : rows.as_array()) {
                const auto& security = row.at("security");
                const auto key = std::make_pair(
                    static_cast<int>(security.at("market_id").as_number()),
                    security.at("code").as_string());
                security_rows[key] = row;
            }
            result["sources"].push_back(
                detail::industry_profile::resource_source_summary(fetched.document));
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = resource;
            failure["message"] = error.what();
            result["detail_errors"].push_back(std::move(failure));
        }
    }

    const bool filter_selected_members = blocks_.at(selected_code).level >= 3;
    const auto selected_members = block_members_.find(selected_code);
    for (const auto& [key, row] : security_rows) {
        if (security_mode && key != std::make_pair(selected_market, options.code)) continue;
        if (!security_mode && filter_selected_members &&
            (selected_members == block_members_.end() || !selected_members->second.count(key)))
            continue;
        if (static_cast<int>(result["shareholder_securities"].size()) >= options.detail_limit)
            break;
        result["shareholder_securities"].push_back(row);
    }
    if (security_mode && result["shareholder_securities"].size())
        result["selected_security_profile"] =
            result["shareholder_securities"].as_array().front();

    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["detail_refreshed"] = any_detail_refreshed;
    cache["detail_age_seconds"] = greatest_detail_age;
    const auto upstream_health = jsn_sources_health(result.at("sources"));
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    result["counts"]["holding_history_points"] =
        static_cast<std::uint64_t>(result["holdings_history"].size());
    result["counts"]["selected_shareholder_industries"] =
        static_cast<std::uint64_t>(result["shareholder_industries"].size());
    result["counts"]["shareholder_securities"] =
        static_cast<std::uint64_t>(result["shareholder_securities"].size());
    result["counts"]["detail_errors"] =
        static_cast<std::uint64_t>(result["detail_errors"].size());
    if (!options.include_catalog) {
        result["industries"] = Json::array();
        result["tree"] = Json::array();
    }
    return result;
}

}  // namespace tdx

