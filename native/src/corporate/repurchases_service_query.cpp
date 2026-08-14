#include "repurchases_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>

namespace tdx {
namespace {

using namespace detail::repurchases;

struct Selection {
    bool security_mode{};
    int market{};
    bool overseas{};
};

Selection validate_query(const RepurchaseQuery& options,
                         const ViewSpec& view) {
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("detail_limit must be in 1..5000");
    if (options.master_cache_ttl_seconds < 0 ||
        options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 ||
        options.detail_cache_ttl_seconds > 86400)
        throw Error("repurchase cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    if (!options.year.empty() && !valid_year(options.year))
        throw Error("year must use YYYY");
    Selection result;
    result.security_mode = !options.market.empty() || !options.code.empty();
    if (result.security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    result.market = result.security_mode ? market_id(options.market) : 0;
    result.overseas = result.market >= 10;
    if (result.security_mode && !digits(options.code, result.overseas ? 5 : 6))
        throw Error(result.overseas
            ? "overseas security code must contain five digits"
            : "code must contain six digits");
    if (result.security_mode && view.kind == ViewKind::plans && result.overseas)
        throw Error("use view=hong-kong for a Hong Kong security");
    if (result.security_mode && view.kind == ViewKind::hong_kong && !result.overseas)
        throw Error("hong-kong view requires an overseas market number");
    return result;
}

Json security_rows(const Json& rows, const Selection& selection,
                   const std::string& code) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        if (selection.security_mode) {
            const auto& security = row.at("security");
            if (static_cast<int>(security.at("market_id").as_number()) != selection.market ||
                text_value(security, "code") != code) continue;
        }
        result.push_back(row);
    }
    return result;
}

void append_error(Json& errors, const std::string& resource,
                  const std::exception& error) {
    Json failure = Json::object();
    failure["resource"] = resource;
    failure["message"] = error.what();
    errors.push_back(std::move(failure));
}

}  // namespace

Json RepurchaseService::query(const RepurchaseQuery& options) {
    using namespace detail::repurchases;
    const auto& view = view_spec(options.view);
    const auto& annual_segment = annual_spec(options.segment);
    const auto selection = validate_query(options, view);
    auto core = fetch_core(options);
    Json plans = Json::array(), monthly = Json::array(), annual_rows = Json::array();
    Json annual_monthly = Json::array(), hong_kong = Json::array();
    Json hong_kong_history = Json::array(), errors = Json::array();
    Json extra_sources = Json::array();
    bool any_detail_refreshed = false;
    int greatest_detail_age = 0;

    auto matched_plans = security_rows(
        core.document.at("plans"), selection, options.code);
    if (view.kind == ViewKind::plans ||
        (selection.security_mode && !selection.overseas))
        plans = limited_filtered(matched_plans, options.query, options.limit);
    if (view.kind == ViewKind::monthly) monthly = core.document.at("monthly");

    std::string selected_year = options.year;
    if (view.kind == ViewKind::annual) {
        annual_rows = core.document.at(std::string("annual_") + annual_segment.id);
        if (selected_year.empty() && annual_rows.size())
            selected_year = text_value(annual_rows.as_array().front(), "year");
        if (!selected_year.empty()) {
            const auto resource = std::string(annual_segment.detail_namespace) + "/" +
                                  selected_year + ".jsn";
            try {
                auto fetched = fetch_resource(resource, options);
                extra_sources.push_back(source_summary(fetched.document));
                any_detail_refreshed = any_detail_refreshed || fetched.refreshed;
                greatest_detail_age = std::max(greatest_detail_age, fetched.age_seconds);
                annual_monthly = normalize_repurchase_annual_rows(
                    fetched.document.at("rows"), annual_segment.id);
                for (auto& row : annual_monthly.as_array()) {
                    row["month"] = text_value(row, "year");
                    row.as_object().erase("year");
                }
                if (static_cast<int>(annual_monthly.size()) > options.detail_limit)
                    annual_monthly.as_array().resize(
                        static_cast<std::size_t>(options.detail_limit));
            } catch (const std::exception& error) {
                append_error(errors, resource, error);
            }
        }
    }

    auto matched_hk = security_rows(
        core.document.at("hong_kong"), selection, options.code);
    if (view.kind == ViewKind::hong_kong)
        hong_kong = limited_filtered(matched_hk, options.query, options.limit);
    if (selection.security_mode && selection.overseas &&
        (options.include_details || view.kind == ViewKind::hong_kong)) {
        const auto resource = "gghg/" + std::to_string(selection.market) +
                              options.code + ".jsn";
        try {
            auto fetched = fetch_resource(resource, options);
            extra_sources.push_back(source_summary(fetched.document));
            any_detail_refreshed = any_detail_refreshed || fetched.refreshed;
            greatest_detail_age = std::max(greatest_detail_age, fetched.age_seconds);
            hong_kong_history = normalize_hk_repurchase_rows(
                fetched.document.at("rows"), securities_, false);
            if (static_cast<int>(hong_kong_history.size()) > options.detail_limit)
                hong_kong_history.as_array().resize(
                    static_cast<std::size_t>(options.detail_limit));
        } catch (const std::exception& error) {
            append_error(errors, resource, error);
        }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-repurchases-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = selection.security_mode ? "security" :
        view.kind == ViewKind::annual ? "year" : "catalog";
    result["plans"] = std::move(plans);
    result["monthly"] = std::move(monthly);
    result["annual"] = std::move(annual_rows);
    result["annual_monthly"] = std::move(annual_monthly);
    result["hong_kong"] = std::move(hong_kong);
    result["hong_kong_history"] = std::move(hong_kong_history);
    result["selected_security"] = selection.security_mode
        ? security_document(selection.market, options.code, securities_) : Json(nullptr);
    result["selected_year"] = selected_year;
    result["summary"] = core.document.at("summary");
    result["matched_plan_summary"] = summarize_plans(matched_plans);
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["query"] = options.query;
    filters["segment"] = options.segment;
    filters["year"] = selected_year;
    result["filters"] = std::move(filters);
    Json sources = core.document.at("sources");
    for (const auto& source : extra_sources.as_array()) sources.push_back(source);
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = core.refreshed;
    cache["master_age_seconds"] = core.age_seconds;
    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["detail_refreshed"] = any_detail_refreshed;
    cache["detail_age_seconds"] = greatest_detail_age;
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["plans"] = static_cast<std::uint64_t>(result.at("plans").size());
    counts["monthly"] = static_cast<std::uint64_t>(result.at("monthly").size());
    counts["annual"] = static_cast<std::uint64_t>(result.at("annual").size());
    counts["annual_monthly"] = static_cast<std::uint64_t>(result.at("annual_monthly").size());
    counts["hong_kong"] = static_cast<std::uint64_t>(result.at("hong_kong").size());
    counts["hong_kong_history"] = static_cast<std::uint64_t>(
        result.at("hong_kong_history").size());
    counts["detail_errors"] = static_cast<std::uint64_t>(
        result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

}  // namespace tdx
