#include "ratings_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <utility>

namespace tdx {

using namespace ratings_detail;

Json RatingService::query(const RatingQuery& options) {
    const std::set<std::string> views{"hong-kong", "us", "industries"};
    const std::set<std::string> stances{
        "all", "positive", "neutral", "negative", "unknown"};
    if (!views.count(options.view))
        throw Error("view must be hong-kong, us, or industries");
    if (!stances.count(options.stance))
        throw Error("stance must be all, positive, neutral, negative, or unknown");
    if (options.limit < 1 || options.limit > 5000 ||
        options.detail_limit < 1 || options.detail_limit > 10000)
        throw Error("rating limits are outside the supported range");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    if (options.view != "industries" && !options.industry.empty())
        throw Error("industry is only valid for the industries view");
    if (options.view == "hong-kong" && !options.code.empty()) {
        const auto market = lower_ascii(trim(options.market));
        if (market != "hk" && market != "31")
            throw Error("Hong Kong rating security market must be hk or 31");
        if (!digits(options.code, 5)) throw Error("Hong Kong code must contain five digits");
    }
    if (options.view == "us" && !options.code.empty()) {
        const auto market = lower_ascii(trim(options.market));
        if (market != "us" && market != "74")
            throw Error("United States rating security market must be us or 74");
        if (!united_states_symbol(upper_ascii(trim(options.code))))
            throw Error("United States symbol contains unsupported characters");
    }
    if (options.view == "industries" && !options.code.empty() &&
        !digits(options.code, 6))
        throw Error("A-share code must contain six digits");
    if (!options.industry.empty() && !digits(options.industry, 6))
        throw Error("industry code must contain six digits");

    auto master = fetch_master(options);
    Json hong_kong = Json::array(), united_states = Json::array();
    Json industries = Json::array(), reports = Json::array();
    Json selected_hong_kong = Json(nullptr), selected_industry = Json(nullptr);
    Json selected_united_states = Json(nullptr);
    Json selected_security = Json(nullptr), errors = Json::array();
    std::string selected_industry_code = options.industry;
    bool selection_mode = false;

    if (options.view == "hong-kong") {
        selection_mode = !options.code.empty();
        for (const auto& row : master.document.at("hong_kong").as_array()) {
            if (selection_mode && text_value(row.at("security"), "code") == options.code) {
                selected_hong_kong = row;
                break;
            }
        }
        Json candidates = selection_mode ? Json::array() : master.document.at("hong_kong");
        if (selection_mode && !selected_hong_kong.is_null())
            candidates.push_back(selected_hong_kong);
        hong_kong = filtered_rows(candidates, options.query, options.stance, options.limit);
    } else if (options.view == "us") {
        const auto code = upper_ascii(trim(options.code));
        selection_mode = !code.empty();
        for (const auto& row : master.document.at("united_states").as_array()) {
            if (selection_mode && text_value(row.at("security"), "code") == code) {
                selected_united_states = row;
                break;
            }
        }
        Json candidates = selection_mode ? Json::array()
                                         : master.document.at("united_states");
        if (selection_mode && !selected_united_states.is_null())
            candidates.push_back(selected_united_states);
        united_states = filtered_rows(
            candidates, options.query, options.stance, options.limit);
    } else {
        if (!options.code.empty()) {
            const auto market = mainland_market_id(options.market);
            selected_security = mainland_security_document(
                market, options.code, securities_);
            const auto found = security_industries_.find({market, options.code});
            if (found != security_industries_.end()) selected_industry_code = found->second;
            else {
                Json failure = Json::object();
                failure["resource"] = nullptr;
                failure["message"] = "selected security has no mapped level-one research industry";
                errors.push_back(std::move(failure));
            }
        }
        selection_mode = !selected_industry_code.empty();
        for (const auto& row : master.document.at("industries").as_array()) {
            if (selection_mode &&
                text_value(row.at("industry"), "code") == selected_industry_code) {
                selected_industry = row;
                break;
            }
        }
        Json candidates = selection_mode ? Json::array() : master.document.at("industries");
        if (selection_mode && !selected_industry.is_null())
            candidates.push_back(selected_industry);
        industries = filtered_rows(candidates, options.query, options.stance, options.limit);
    }

    Json sources = master.document.at("sources");
    bool detail_refreshed = false;
    int detail_age = 0;
    if (selection_mode && options.include_details) {
        const bool missing = options.view == "hong-kong" ? selected_hong_kong.is_null()
            : options.view == "us" ? selected_united_states.is_null()
                                    : selected_industry.is_null();
        if (missing) {
            Json failure = Json::object();
            failure["resource"] = nullptr;
            failure["message"] = options.view == "hong-kong"
                ? "selected Hong Kong security is absent from the six-month rating summary"
                : options.view == "us"
                    ? "selected United States security is absent from the current rating summary"
                    : "selected industry is absent from the six-month rating summary";
            errors.push_back(std::move(failure));
        } else {
            const auto resource = options.view == "hong-kong"
                ? "ggpj/31" + options.code + ".jsn"
                : options.view == "us"
                    ? "mgpj/74" + upper_ascii(trim(options.code)) + ".jsn"
                    : "hypj/1" + selected_industry_code + ".jsn";
            try {
                auto detail = fetch_resource(resource, options);
                detail_refreshed = detail.refreshed;
                detail_age = detail.age_seconds;
                sources.push_back(source_summary(detail.document));
                const auto normalized = normalize_rating_report_rows(
                    detail.document.at("rows"), options.view);
                int returned = 0;
                for (const auto& report : normalized.as_array()) {
                    if (returned++ >= options.detail_limit) break;
                    reports.push_back(report);
                }
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["resource"] = resource;
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
        }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-ratings-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = selection_mode ? "selection" : "catalog";
    result["hong_kong"] = std::move(hong_kong);
    result["united_states"] = std::move(united_states);
    result["industries"] = std::move(industries);
    result["reports"] = std::move(reports);
    result["selected_hong_kong"] = std::move(selected_hong_kong);
    result["selected_united_states"] = std::move(selected_united_states);
    result["selected_industry"] = std::move(selected_industry);
    result["selected_security"] = std::move(selected_security);
    result["hong_kong_summary"] = master.document.at("hong_kong_summary");
    result["united_states_summary"] = master.document.at("united_states_summary");
    result["industry_summary"] = master.document.at("industry_summary");
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["stance"] = options.stance;
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["hong_kong"] = static_cast<std::uint64_t>(result.at("hong_kong").size());
    counts["united_states"] = static_cast<std::uint64_t>(
        result.at("united_states").size());
    counts["industries"] = static_cast<std::uint64_t>(result.at("industries").size());
    counts["reports"] = static_cast<std::uint64_t>(result.at("reports").size());
    counts["detail_errors"] =
        static_cast<std::uint64_t>(result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

}  // namespace tdx
