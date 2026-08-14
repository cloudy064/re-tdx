#include "forecasts_internal.hpp"

#include "tdx/common.hpp"

namespace tdx {
namespace {

struct QueryOutput {
    Json industries{Json::array()};
    Json securities{Json::array()};
    Json hong_kong{Json::array()};
    Json errors{Json::array()};
    Json extra_sources{Json::array()};
    bool detail_refreshed{};
    int detail_age{};
};

void append_error(Json& errors, const Json& resource,
                  const std::string& message) {
    Json failure = Json::object();
    failure["resource"] = resource;
    failure["message"] = message;
    errors.push_back(std::move(failure));
}

Json matching_industries(const Json& rows, const std::string& industry,
                         const std::string& report_period) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = forecast_detail::text_value(row.at("industry"), "code");
        if (!industry.empty() && code != industry) continue;
        if (!report_period.empty() &&
            forecast_detail::text_value(row, "report_period") != report_period)
            continue;
        result.push_back(row);
    }
    return result;
}

}  // namespace

Json ForecastService::query(const ForecastQuery& options) {
    using namespace forecast_detail;
    const auto* view = find_view(options.view);
    if (!view)
        throw Error("view must be industries, securities, latest, or hong-kong");
    if (!valid_category(options.category))
        throw Error("category must be all, positive, negative, or uncertain");
    if (options.limit < 1 || options.limit > 5000 ||
        options.detail_limit < 1 || options.detail_limit > 10000)
        throw Error("forecast limits are outside the supported range");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const bool security_mode = !options.code.empty();
    const int selected_market = security_mode
        ? mainland_market_id(options.market) : -1;
    if (security_mode && !digits(options.code, 6))
        throw Error("code must contain six digits");
    if (!options.industry.empty() && !forecast_group_code(options.industry))
        throw Error("industry must be a six-digit 881xxx code or 880001 market summary");
    if (!options.report_period.empty() && !digits(options.report_period, 8))
        throw Error("report_period must use YYYYMMDD");

    auto core = fetch_core(options);
    QueryOutput output;
    Json selected_industry = Json(nullptr);
    Json selected_security = security_mode
        ? security_document(selected_market, options.code, securities_)
        : Json(nullptr);

    std::string resolved_industry = options.industry;
    if (security_mode && resolved_industry.empty()) {
        const auto found = security_industries_.find(
            {selected_market, options.code});
        if (found != security_industries_.end())
            resolved_industry = found->second;
    }
    const auto matches = matching_industries(
        core.document.at("industries"), resolved_industry,
        options.report_period);
    if (!matches.as_array().empty())
        selected_industry = matches.as_array().front();

    switch (view->kind) {
    case ViewKind::industries: {
        Json candidates = options.industry.empty()
            ? core.document.at("industries") : matches;
        if (!options.report_period.empty() && options.industry.empty()) {
            candidates = matching_industries(
                candidates, "", options.report_period);
        }
        output.industries = limited_filtered(
            candidates, options.query, options.limit);
        break;
    }
    case ViewKind::securities: {
        if (resolved_industry.empty()) {
            append_error(output.errors, Json(nullptr), security_mode
                ? "selected security has no first-level research-industry mapping"
                : "securities view requires --industry or --market with --code");
        } else if (matches.as_array().empty()) {
            append_error(output.errors, Json(nullptr),
                         "selected industry has no current forecast-statistics row");
        } else if (options.include_details) {
            const auto detail_id = text_value(matches.as_array().front(), "detail_id");
            const auto resource = "yjyg/" + detail_id + ".jsn";
            try {
                auto detail = fetch_resource(resource, options);
                output.extra_sources.push_back(source_summary(detail.document));
                output.detail_refreshed = detail.refreshed;
                output.detail_age = detail.age_seconds;
                const auto normalized = normalize_forecast_security_rows(
                    detail.document.at("rows"), securities_);
                Json selected = Json::array();
                for (const auto& row : normalized.as_array()) {
                    if (security_mode) {
                        const auto& security = row.at("security");
                        if (static_cast<int>(security.at("market_id").as_number()) !=
                                selected_market ||
                            text_value(security, "code") != options.code)
                            continue;
                    }
                    selected.push_back(row);
                }
                output.securities = limited_filtered(
                    selected, options.query, options.detail_limit,
                    options.category);
            } catch (const std::exception& error) {
                append_error(output.errors, Json(resource), error.what());
            }
        }
        break;
    }
    case ViewKind::latest: {
        Json candidates = Json::array();
        for (const auto& row : core.document.at("latest").as_array()) {
            if (security_mode) {
                const auto& security = row.at("security");
                if (static_cast<int>(security.at("market_id").as_number()) !=
                        selected_market ||
                    text_value(security, "code") != options.code)
                    continue;
            }
            if (!options.report_period.empty() &&
                text_value(row, "report_period") != options.report_period)
                continue;
            candidates.push_back(row);
        }
        output.securities = limited_filtered(
            candidates, options.query, options.limit, options.category);
        break;
    }
    case ViewKind::hong_kong: {
        Json candidates = Json::array();
        for (const auto& row : core.document.at("hong_kong").as_array()) {
            if (!options.report_period.empty() &&
                text_value(row, "end_date") != options.report_period)
                continue;
            candidates.push_back(row);
        }
        output.hong_kong = limited_filtered(
            candidates, options.query, options.limit, options.category);
        break;
    }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-forecasts-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = security_mode ? "security" :
        !resolved_industry.empty() ? "industry" : "catalog";
    result["industries"] = std::move(output.industries);
    result["securities"] = std::move(output.securities);
    result["hong_kong"] = std::move(output.hong_kong);
    result["selected_industry"] = std::move(selected_industry);
    result["selected_security"] = std::move(selected_security);
    result["summary"] = core.document.at("summary");
    result["detail_errors"] = std::move(output.errors);
    Json filters = Json::object();
    filters["category"] = options.category;
    filters["query"] = options.query;
    filters["industry"] = resolved_industry;
    filters["report_period"] = options.report_period;
    result["filters"] = std::move(filters);
    Json sources = core.document.at("sources");
    for (const auto& source : output.extra_sources.as_array())
        sources.push_back(source);
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_refreshed"] = core.refreshed;
    cache["master_age_seconds"] = core.age_seconds;
    cache["detail_refreshed"] = output.detail_refreshed;
    cache["detail_age_seconds"] = output.detail_age;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["industries"] =
        static_cast<std::uint64_t>(result.at("industries").size());
    counts["securities"] =
        static_cast<std::uint64_t>(result.at("securities").size());
    counts["hong_kong"] =
        static_cast<std::uint64_t>(result.at("hong_kong").size());
    counts["detail_errors"] =
        static_cast<std::uint64_t>(result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

}  // namespace tdx
