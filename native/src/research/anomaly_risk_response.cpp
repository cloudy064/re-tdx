#include "anomaly_risk_internal.hpp"

#include "tdx/cloud_workflow.hpp"

namespace tdx::detail::anomaly_risk {

Json build_live_document(const Json& upstream, const QueryPlan& plan,
                         const BlockData& blocks) {
    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    const auto normalized = plan.view->normalize(raw_rows, blocks);
    Json records = Json::array();
    const auto wanted = lower_ascii(plan.options.query);
    std::map<std::string, std::size_t> by_market, by_board, by_warning;
    std::size_t upstream_warnings = 0;
    for (const auto& record : normalized.as_array()) {
        const auto& security = record.at("security");
        const auto record_market = text(security, "market");
        const auto record_code = text(security, "code");
        const auto warning_active = record.at("warning").at("active").as_bool();
        const auto warning_status = text(record.at("warning"), "status");
        if (warning_active) ++upstream_warnings;
        if (!plan.options.market.empty() && record_market != plan.options.market) continue;
        if (!plan.options.code.empty() && record_code != plan.options.code) continue;
        if (plan.options.warnings_only && !warning_active) continue;
        if (plan.warning_filter && warning_status != plan.warning_filter->status) continue;
        if (!wanted.empty() &&
            lower_ascii(record.dump(-1)).find(wanted) == std::string::npos)
            continue;
        ++by_market[record_market];
        ++by_board[text(record, "listing_board")];
        ++by_warning[warning_status];
        records.push_back(record);
    }
    const auto matched = records.size();
    const bool truncated = records.size() >
        static_cast<std::size_t>(plan.options.limit);
    if (truncated)
        records.as_array().resize(static_cast<std::size_t>(plan.options.limit));

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["upstream_warning_rows"] = static_cast<std::uint64_t>(upstream_warnings);
    counts["matched_rows"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    auto counts_document = [](const auto& values) {
        Json result = Json::object();
        for (const auto& [name, count] : values)
            result[name] = static_cast<std::uint64_t>(count);
        return result;
    };

    Json parameters = Json::object();
    parameters["query"] = plan.options.query;
    parameters["market"] = plan.options.market.empty()
        ? Json(nullptr) : Json(plan.options.market);
    parameters["code"] = plan.options.code.empty()
        ? Json(nullptr) : Json(plan.options.code);
    parameters["warning"] = plan.options.warning;
    parameters["warnings_only"] = plan.options.warnings_only;

    Json units = Json::object();
    units["returns_deviations_and_standards"] = "percentage-points";
    units["suspension_reported_deviation_ratio"] =
        "ratio; deviation_pct is the ratio multiplied by 100";
    units["prices"] = "CNY or index-points according to entity";
    Json boundary = Json::object();
    boundary["host_live_columns"] =
        "2044 N023-N028 are server-enriched subject/index quote fields; values may be intraday.";
    boundary["unresolved_statistics_columns"] =
        "2044 N029-N037 are retained under upstream_auxiliary_unresolved without guessed labels.";
    boundary["investment_signal"] = false;
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = plan.options.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-anomaly-risk-native-v1";
    result["availability"] = records.size() ? "live" : "empty";
    result["generated_at"] = now_text();
    result["view"] = plan.view->id;
    result["view_title"] = plan.view->title;
    result["available_views"] = available_views();
    result["parameters"] = std::move(parameters);
    result["units"] = std::move(units);
    result["field_boundary"] = std::move(boundary);
    result["warning_catalog"] = warning_catalog();
    result["counts"] = std::move(counts);
    result["counts_by_market"] = counts_document(by_market);
    result["counts_by_board"] = counts_document(by_board);
    result["counts_by_warning"] = counts_document(by_warning);
    result["records"] = std::move(records);
    result["source"] = source_document(upstream, *plan.view, raw_rows.size());
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    result["unresolved_field_values_retained"] =
        plan.view->retains_unresolved_fields;
    return result;
}

}  // namespace tdx::detail::anomaly_risk
