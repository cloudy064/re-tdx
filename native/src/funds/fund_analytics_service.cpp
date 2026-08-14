#include "fund_analytics_internal.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace fund_detail = fund_analytics_detail;

FundAnalyticsService::FundAnalyticsService(fs::path root)
    : root_(std::move(root)) {}

Json FundAnalyticsService::query(const FundAnalyticsQuery& input) {
    FundAnalyticsQuery query = input;
    query.view = lower_ascii(trim(query.view));
    query.query = trim(query.query);
    query.fund_code = trim(query.fund_code);
    query.style = trim(query.style);
    query.report_date = trim(query.report_date);
    const auto* definition = fund_detail::view_definition(query.view);
    if (!definition) throw Error("unknown fund-analytics view");
    const auto* style = fund_detail::style_definition(query.style);
    if (!style) throw Error("style is not a client fund-style code");
    if (query.fund_size < 0 || query.fund_size > 6 ||
        query.fund_age < 0 || query.fund_age > 6 ||
        query.benchmark < 0 || query.benchmark > 2 ||
        !std::isfinite(query.risk_free_rate) || query.risk_free_rate < 0 ||
        query.risk_free_rate > 100 || query.limit < 1 || query.limit > 20000 ||
        query.max_pages < 1 || query.max_pages > 100 ||
        query.cache_ttl_seconds < 0 || query.cache_ttl_seconds > 3600 ||
        query.timeout_ms < 100 || query.timeout_ms > 60000)
        throw Error("fund-analytics query limits are invalid");
    if (!query.fund_code.empty() &&
        (query.fund_code.size() != 6 || !fund_detail::digits(query.fund_code)))
        throw Error("fund code must contain exactly six digits");
    if (definition->requires_fund && query.fund_code.empty())
        throw Error("selected fund-analytics view requires fund_code");

    const auto today = fund_detail::today_local();
    const bool report_date_explicit = !query.report_date.empty();
    if (query.view == "risk" || query.view == "risk-history" ||
        query.view == "selection-skill") {
        if (query.start_date.empty())
            query.start_date = fund_detail::date_text(
                fund_detail::shift_months(today, -3));
        if (query.end_date.empty())
            query.end_date = fund_detail::date_text(today);
    } else if (query.view == "monthly-risk" ||
               query.view == "monthly-history") {
        if (query.start_date.empty())
            query.start_date = fund_detail::date_text(
                fund_detail::shift_years(today, -3));
        if (query.end_date.empty())
            query.end_date = fund_detail::date_text(today);
    } else if (query.view == "holdings-stability" ||
               query.view == "holding-industries" ||
               query.view == "holding-history") {
        const auto report = fund_detail::latest_full_fund_report(today);
        if (query.start_date.empty())
            query.start_date = fund_detail::date_text(
                fund_detail::shift_years(report, -2));
        if (query.end_date.empty())
            query.end_date = fund_detail::date_text(report);
    } else if (query.view == "reported-holdings" ||
               query.view == "reported-holding-industries" ||
               query.view == "reported-holding-securities") {
        if (query.report_date.empty())
            query.report_date = fund_detail::date_text(
                fund_detail::latest_full_fund_report(today));
    } else if (query.view == "market-position-history") {
        if (query.start_date.empty())
            query.start_date = fund_detail::date_text(
                fund_detail::shift_years(today, -1));
        if (query.end_date.empty())
            query.end_date = fund_detail::date_text(today);
    }
    if (!query.start_date.empty())
        query.start_date = fund_detail::compact_date(
            query.start_date, "start date");
    if (!query.end_date.empty())
        query.end_date = fund_detail::compact_date(query.end_date, "end date");
    if (!query.report_date.empty())
        query.report_date = fund_detail::compact_date(
            query.report_date, "report date");
    if (!query.start_date.empty() && query.start_date > query.end_date)
        throw Error("start date must not be later than end date");
    const bool estimate_date_explicit = !query.estimate_date.empty();
    if (query.view == "position-estimates") {
        if (query.estimate_date.empty())
            query.estimate_date = fund_detail::date_text(today);
        query.estimate_date = fund_detail::compact_date(
            query.estimate_date, "estimate date");
    }

    const auto key = fund_detail::cache_key(query) +
        "|report-date-explicit=" + (report_date_explicit ? "1" : "0");
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!query.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        if (age < query.cache_ttl_seconds) {
            auto document = cached->second.document;
            document["cache"]["hit"] = true;
            document["cache"]["age_seconds"] = age;
            return document;
        }
    }

    std::ostringstream risk_free;
    risk_free << std::setprecision(12) << query.risk_free_rate;
    std::map<std::string, std::string> replacements{
        {"style_details", query.style},
        {"fund_size", std::to_string(query.fund_size)},
        {"fund_setup_time", std::to_string(query.fund_age)},
        {"basic_code", std::to_string(query.benchmark)},
        {"start_date", query.start_date},
        {"end_date", query.end_date},
        {"report_date", query.report_date},
        {"rate_freerisk", risk_free.str()},
        {"fund_code", query.fund_code},
        {"estimate_date", query.estimate_date},
    };
    auto fetch = [&](bool all_pages) {
        Json upstream;
        for (int attempt = 0; attempt < 3; ++attempt) {
            try {
                upstream = execute_tqlex_config(
                    root_, definition->request_id, replacements, {}, {},
                    definition->source_file, {},
                    all_pages && definition->paged,
                    all_pages && definition->paged ? 0 : -1,
                    all_pages && definition->paged ? 50 : 0,
                    query.max_pages,
                    cloud_endpoints::tqlex, query.timeout_ms);
                return upstream;
            } catch (const Error& error) {
                if (!fund_detail::transient_error(error.what()) || attempt == 2)
                    throw;
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(250 * (attempt + 1)));
            }
        }
        return upstream;
    };

    bool latest_fallback_used = false;
    bool report_fallback_used = false;
    const std::string requested_report_date = query.report_date;
    std::string requested_estimate_date = query.estimate_date;
    Json upstream;
    if ((query.view == "reported-holding-industries" ||
         query.view == "reported-holding-securities") &&
        !report_date_explicit) {
        for (int period = 0; period < 5; ++period) {
            replacements["report_date"] = query.report_date;
            upstream = fetch(false);
            if (cloud_result_rows(upstream.at("response")).size()) {
                report_fallback_used = period != 0;
                break;
            }
            if (period != 4)
                query.report_date = fund_detail::previous_full_fund_report(
                    query.report_date);
        }
        report_fallback_used = query.report_date != requested_report_date;
    } else if (query.view == "position-estimates" &&
               !estimate_date_explicit) {
        for (int days = 0; days <= 10; ++days) {
            query.estimate_date = fund_detail::date_days_ago(days);
            replacements["estimate_date"] = query.estimate_date;
            upstream = fetch(false);
            const auto probe = cloud_result_rows(upstream.at("response"));
            bool usable = false;
            for (const auto& row : probe.as_array())
                if (fund_detail::text(row, "estimateDate") != "0" &&
                    !fund_detail::text(row, "estimateDate").empty()) {
                    usable = true;
                    break;
                }
            if (usable) {
                latest_fallback_used = days != 0;
                if (query.all_pages) upstream = fetch(true);
                break;
            }
        }
    } else {
        upstream = fetch(query.all_pages);
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    const auto normalized = normalize_fund_analytics_rows(raw_rows, query.view);
    Json records = Json::array();
    const auto wanted = lower_ascii(query.query);
    for (const auto& record : normalized.as_array()) {
        if (!query.fund_code.empty() && !definition->requires_fund &&
            record.is_object() && record.as_object().count("fund") &&
            record.at("fund").at("code").as_string() != query.fund_code)
            continue;
        if (!wanted.empty() &&
            lower_ascii(record.dump(-1)).find(wanted) == std::string::npos)
            continue;
        records.push_back(record);
    }
    const auto matched = records.size();
    const bool truncated = records.size() >
        static_cast<std::size_t>(query.limit);
    if (truncated)
        records.as_array().resize(static_cast<std::size_t>(query.limit));

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["matched_rows"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    Json parameters = Json::object();
    parameters["view"] = query.view;
    parameters["query"] = query.query;
    parameters["fund_code"] = query.fund_code.empty()
        ? Json(nullptr) : Json(query.fund_code);
    parameters["style_code"] = query.style;
    parameters["style_name"] = std::string(style->name);
    parameters["fund_size_selector"] = query.fund_size;
    parameters["fund_age_selector"] = query.fund_age;
    parameters["benchmark"] = fund_detail::benchmark_document(query.benchmark);
    parameters["start_date"] = query.start_date.empty()
        ? Json(nullptr) : Json(fund_detail::display_date(query.start_date));
    parameters["end_date"] = query.end_date.empty()
        ? Json(nullptr) : Json(fund_detail::display_date(query.end_date));
    parameters["requested_report_date"] = requested_report_date.empty()
        ? Json(nullptr) : Json(fund_detail::display_date(requested_report_date));
    parameters["resolved_report_date"] = query.report_date.empty()
        ? Json(nullptr) : Json(fund_detail::display_date(query.report_date));
    parameters["latest_report_fallback_used"] = report_fallback_used;
    parameters["risk_free_rate_pct"] = query.risk_free_rate;
    parameters["requested_estimate_date"] = requested_estimate_date.empty()
        ? Json(nullptr) : Json(fund_detail::display_date(requested_estimate_date));
    parameters["resolved_estimate_date"] = query.estimate_date.empty()
        ? Json(nullptr) : Json(fund_detail::display_date(query.estimate_date));
    parameters["latest_estimate_fallback_used"] = latest_fallback_used;
    Json pagination = Json::object();
    pagination["all_pages"] = query.all_pages && definition->paged;
    pagination["page_size"] = definition->paged ? Json(50) : Json(nullptr);
    pagination["complete"] = !definition->paged || query.all_pages ||
        raw_rows.size() < 50;
    pagination["service_page_size_boundary"] = definition->paged
        ? Json("50 rows; larger risk tables may exceed the TQLEX output buffer")
        : Json(nullptr);
    Json source = Json::object();
    source["transport"] = "TQLEX reqformat=2";
    source["entry"] = upstream.at("entry");
    source["request_id"] = definition->request_id;
    source["source_file"] = upstream.at("source_file");
    source["module"] = "module_risk_return.dll";
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = query.cache_ttl_seconds;
    Json result = Json::object();
    result["schema"] = "tdx-fund-analytics-native-v2";
    result["availability"] = records.size() ? "live" : "empty";
    result["generated_at"] = fund_detail::now_text();
    result["view"] = query.view;
    result["view_title"] = definition->title;
    result["available_views"] = fund_detail::views_document();
    result["parameters"] = std::move(parameters);
    if (definition->requires_fund) {
        Json selected = Json::object();
        selected["entity_type"] = "fund";
        selected["fund_id"] = "FUND:" + query.fund_code;
        selected["code"] = query.fund_code;
        result["selected_fund"] = std::move(selected);
    } else {
        result["selected_fund"] = Json(nullptr);
    }
    result["counts"] = std::move(counts);
    result["pagination"] = std::move(pagination);
    result["records"] = std::move(records);
    result["source"] = std::move(source);
    result["cache"] = std::move(cache);
    result["unit_boundary"] =
        "fields suffixed _pct use percentage points; ratios and raw share/market values retain upstream units";
    result["monthly_history_unit_conversion"] =
        query.view == "monthly-history"
            ? Json("fund and benchmark returns are upstream ratios multiplied by 100; excess returns are already percentage points")
            : Json(nullptr);
    result["reported_holdings_boundary"] =
        query.view.rfind("reported-holding", 0) == 0 ||
        query.view == "reported-holdings"
            ? Json("public report-period holdings; not complete real-time positions; detail views may fall back to the latest period with rows")
            : Json(nullptr);
    result["investment_signal"] = false;
    result["raw_response_retained"] = false;
    cache_[key] = {result, std::time(nullptr)};
    return result;
}

}  // namespace tdx
