#include "flow_followup_internal.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/pbrpc.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace flow_followup_detail;

FlowFollowupService::FlowFollowupService(fs::path root) : root_(std::move(root)) {}

Json FlowFollowupService::query(const FlowFollowupQuery& input) {
    FlowFollowupQuery options = input;
    options.view = canonical_view(options.view);
    const auto default_start = options.view == "margin" ? "20171115" : "20170101";
    options.start_date = compact_date(options.start_date.empty()
        ? default_start : options.start_date, "start_date");
    options.end_date = compact_date(options.end_date.empty()
        ? today_text() : options.end_date, "end_date");
    if (options.start_date > options.end_date)
        throw Error("start_date must not exceed end_date");
    if (options.limit < 1 || options.limit > 20000 ||
        options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400 ||
        options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("flow follow-up query limits are invalid");

    const auto key = cache_key(options);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!options.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
        if (age < options.cache_ttl_seconds) {
            auto result = cached->second.document;
            result["cache"]["hit"] = true;
            result["cache"]["age_seconds"] = age;
            return result;
        }
    }

    if (!history_view(options.view)) {
        if (options.available_only)
            throw Error("available_only is only valid for margin/northbound history views");
        const auto& definition = model_definition(options.view);
        const std::map<std::string, std::string> replacements{
            {"StartDate", options.start_date}, {"EndDate", options.end_date}};
        const std::vector<std::string> selector{
            "'Type': '" + std::string(definition.request_type) + "'"};
        Json bucket_upstream, summary_upstream, counterpart_upstream;
        for (int attempt = 0; attempt < 3; ++attempt) {
            try {
                bucket_upstream = execute_tqlex_config(
                    root_, definition.bucket_request_id, replacements, {}, {},
                    definition.source_file, selector, false, -1, 0, 100,
                    cloud_endpoints::tqlex, options.timeout_ms);
                summary_upstream = execute_tqlex_config(
                    root_, definition.summary_request_id, replacements, {}, {},
                    definition.source_file, selector, false, -1, 0, 100,
                    cloud_endpoints::tqlex, options.timeout_ms);
                if (definition.northbound) {
                    const std::string counterpart_type =
                        std::string(definition.request_type) == "1" ? "2" : "1";
                    counterpart_upstream = execute_tqlex_config(
                        root_, definition.summary_request_id, replacements, {}, {},
                        definition.source_file,
                        {"'Type': '" + counterpart_type + "'"}, false, -1, 0, 100,
                        cloud_endpoints::tqlex, options.timeout_ms);
                }
                break;
            } catch (const Error& error) {
                const std::string message = error.what();
                if (!transient_error(message)) throw;
                if (attempt == 2) {
                    if (cached != cache_.end()) {
                        auto stale = cached->second.document;
                        stale["availability"] = "stale-cache";
                        stale["cache"]["hit"] = true;
                        stale["cache"]["stale"] = true;
                        stale["cache"]["age_seconds"] = static_cast<std::uint64_t>(
                            std::max<std::time_t>(0, now - cached->second.fetched_at));
                        stale["cache"]["upstream_error"] = message;
                        return stale;
                    }
                    throw;
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(250 * (attempt + 1)));
            }
        }

        const auto raw_buckets = cloud_result_rows(bucket_upstream.at("response"));
        const auto raw_summary = cloud_result_rows(summary_upstream.at("response"));
        auto records = normalize_flow_model_rows(options.view, raw_buckets);
        auto current_signal = normalize_flow_model_summary(options.view, raw_summary);
        Json sources = Json::array();
        sources.push_back(tqlex_source_document(
            bucket_upstream, definition.bucket_request_id, definition.module,
            raw_buckets.size()));
        sources.push_back(tqlex_source_document(
            summary_upstream, definition.summary_request_id, definition.module,
            raw_summary.size()));

        Json placeholder_audit = Json(nullptr);
        if (definition.northbound) {
            const auto counterpart_view = options.view == "northbound-inflow-model"
                ? "northbound-purchase-model" : "northbound-inflow-model";
            const auto counterpart_rows = cloud_result_rows(
                counterpart_upstream.at("response"));
            const auto counterpart = normalize_flow_model_summary(
                counterpart_view, counterpart_rows);
            sources.push_back(tqlex_source_document(
                counterpart_upstream, definition.summary_request_id, definition.module,
                counterpart_rows.size()));
            const auto selected_value = number(current_signal, "flow_100m_cny");
            const auto counterpart_value = number(counterpart, "flow_100m_cny");
            const auto selected_date = text(current_signal, "date");
            const auto counterpart_date = text(counterpart, "date");
            const auto inflow = options.view == "northbound-inflow-model"
                ? selected_value : counterpart_value;
            const auto purchase = options.view == "northbound-purchase-model"
                ? selected_value : counterpart_value;
            const bool placeholder = !selected_date.empty() &&
                selected_date == counterpart_date && inflow && purchase &&
                std::abs(*inflow - 1040.0) < 1e-12 && std::abs(*purchase) < 1e-12;
            placeholder_audit = Json::object();
            placeholder_audit["date"] = selected_date.empty()
                ? Json(nullptr) : Json(selected_date);
            placeholder_audit["reported_net_inflow_100m_cny"] = number_json(inflow);
            placeholder_audit["reported_net_purchase_100m_cny"] = number_json(purchase);
            placeholder_audit["exact_1040_0_pair"] = placeholder;
            placeholder_audit["counterpart_signal_kind"] = counterpart.at("signal_kind");
            if (placeholder) {
                current_signal["signal_available"] = false;
                current_signal["usable_for_signal_analysis"] = false;
                current_signal["data_status"] = "upstream-placeholder-pair";
            }
        }

        Json current_bucket = Json(nullptr);
        std::size_t current_bucket_count = 0;
        for (auto& record : records.as_array()) {
            record["usable_for_current_signal"] =
                record.at("is_current_bucket").as_bool()
                    ? Json(bool_value(current_signal, "signal_available"))
                    : Json(nullptr);
            if (!record.at("is_current_bucket").as_bool()) continue;
            ++current_bucket_count;
            if (current_bucket.is_null()) current_bucket = record;
        }
        if (current_bucket_count > 1)
            throw Error("flow model response marks more than one current bucket");
        const auto eligible = records.size();
        const bool truncated = records.size() > static_cast<std::size_t>(options.limit);
        if (truncated) records.as_array().resize(static_cast<std::size_t>(options.limit));

        Json counts = Json::object();
        counts["upstream_bucket_rows"] = static_cast<std::uint64_t>(raw_buckets.size());
        counts["normalized_buckets"] = static_cast<std::uint64_t>(eligible);
        counts["current_buckets"] = static_cast<std::uint64_t>(current_bucket_count);
        counts["returned"] = static_cast<std::uint64_t>(records.size());
        counts["truncated"] = truncated;

        Json parameters = Json::object();
        parameters["start_date"] = display_date(options.start_date);
        parameters["end_date"] = display_date(options.end_date);
        parameters["request_type"] = definition.request_type;

        Json methodology = Json::object();
        methodology["population"] = "historical observations grouped by signal bucket";
        methodology["index"] = "CSI 300";
        methodology["forward_returns"] =
            "Mean cumulative CSI 300 price change after observations in each bucket.";
        methodology["positive_ratio"] =
            "Percentage of observations whose forward CSI 300 return is positive.";
        methodology["association_not_causation"] = true;
        methodology["investment_signal"] = false;
        methodology["upstream_calls_result_prediction"] = true;

        Json warnings = Json::array();
        if (!bool_value(current_signal, "signal_available"))
            warnings.push_back(
                "The current northbound 1040/0 pair is an upstream placeholder; "
                "bucket history is retained but the current signal is unavailable.");
        if (definition.northbound)
            warnings.push_back(
                "The upstream 500501 response declares RowNum=1/ColNum=2 in some "
                "responses; decoded content and column descriptions are authoritative.");

        Json cache = Json::object();
        cache["hit"] = false; cache["stale"] = false; cache["age_seconds"] = 0;
        cache["ttl_seconds"] = options.cache_ttl_seconds;

        Json result = Json::object();
        result["schema"] = "tdx-flow-followup-native-v2";
        result["availability"] = "live";
        result["generated_at"] = now_text();
        result["view"] = options.view;
        result["view_title"] = definition.title;
        result["available_views"] = available_views();
        result["parameters"] = std::move(parameters);
        result["methodology"] = std::move(methodology);
        result["current_signal"] = std::move(current_signal);
        result["current_bucket"] = std::move(current_bucket);
        result["northbound_placeholder_audit"] = std::move(placeholder_audit);
        result["counts"] = std::move(counts);
        result["records"] = std::move(records);
        result["warnings"] = std::move(warnings);
        result["sources"] = std::move(sources);
        result["cache"] = std::move(cache);
        result["raw_response_retained"] = false;
        cache_[key] = CachedDocument{result, std::time(nullptr)};
        return result;
    }

    const std::string request_id = options.view == "margin" ? "200011" : "500503";
    const std::string source_file = options.view == "margin"
        ? "rzrq_zsyc1.xml" : "bxzj_zsyc4.xml";
    const std::map<std::string, std::string> replacements{
        {"StartDate", options.start_date}, {"EndDate", options.end_date}};
    Json upstream;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            upstream = execute_pbrpc_config(
                root_, request_id, replacements, {}, {}, {}, source_file, {},
                cloud_endpoints::tqlex, options.timeout_ms);
            break;
        } catch (const Error& error) {
            const std::string message = error.what();
            if (!transient_error(message)) throw;
            if (attempt == 2) {
                if (cached != cache_.end()) {
                    auto stale = cached->second.document;
                    stale["availability"] = "stale-cache";
                    stale["cache"]["hit"] = true;
                    stale["cache"]["stale"] = true;
                    stale["cache"]["age_seconds"] = static_cast<std::uint64_t>(
                        std::max<std::time_t>(0, now - cached->second.fetched_at));
                    stale["cache"]["upstream_error"] = message;
                    return stale;
                }
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    auto normalized = normalize_flow_followup_rows(options.view, raw_rows);
    const bool latest_may_be_partial = options.view == "margin" &&
        options.end_date >= today_text() && normalized.size();
    if (latest_may_be_partial)
        normalized.as_array().back()["data_status"] = "latest-may-be-partial";
    std::size_t available = 0, placeholders = 0;
    std::string placeholder_start, last_available_date;
    for (const auto& record : normalized.as_array()) {
        if (bool_value(record, "signal_available")) {
            ++available;
            last_available_date = text(record, "date");
        } else {
            ++placeholders;
            if (placeholder_start.empty()) placeholder_start = text(record, "date");
        }
    }

    Json statistics = Json::object();
    statistics["population"] = "signal-available records";
    statistics["computed_locally"] = true;
    statistics["days_1"] = horizon_statistics(normalized, "days_1");
    statistics["days_3"] = horizon_statistics(normalized, "days_3");
    statistics["days_5"] = horizon_statistics(normalized, "days_5");
    if (options.view == "margin")
        statistics["days_10"] = horizon_statistics(normalized, "days_10");

    Json records = Json::array();
    std::size_t excluded = 0;
    for (const auto& record : normalized.as_array()) {
        if (options.available_only && !bool_value(record, "signal_available")) {
            ++excluded;
            continue;
        }
        records.push_back(record);
    }
    const auto eligible = records.size();
    const bool truncated = records.size() > static_cast<std::size_t>(options.limit);
    if (truncated) {
        records.as_array().erase(
            records.as_array().begin(),
            records.as_array().end() - static_cast<std::ptrdiff_t>(options.limit));
    }

    Json summary = Json::object();
    summary["observed_start_date"] = normalized.size()
        ? normalized.as_array().front().at("date") : Json(nullptr);
    summary["observed_end_date"] = normalized.size()
        ? normalized.as_array().back().at("date") : Json(nullptr);
    summary["last_signal_available_date"] = last_available_date.empty()
        ? Json(nullptr) : Json(last_available_date);
    summary["placeholder_start_date"] = placeholder_start.empty()
        ? Json(nullptr) : Json(placeholder_start);
    summary["latest"] = records.size() ? records.as_array().back() : Json(nullptr);
    summary["forward_return_statistics"] = std::move(statistics);

    Json warnings = Json::array();
    if (latest_may_be_partial)
        warnings.push_back("The newest trading day may contain partial market disclosure.");
    if (placeholders)
        warnings.push_back(
            "A long repeated 1040/0 northbound pair is retained as reported evidence but "
            "nulled as a usable signal.");

    Json methodology = Json::object();
    methodology["index"] = "CSI 300";
    methodology["forward_returns"] =
        "Cumulative CSI 300 price change after the source trading date.";
    methodology["association_not_causation"] = true;
    methodology["forecast"] = false;
    methodology["client_note"] = options.view == "margin"
        ? "Big-data statistics for reference; newest disclosure may be partial."
        : "Subsequent CSI 300 performance at different northbound net-purchase scales.";

    Json parameters = Json::object();
    parameters["start_date"] = display_date(options.start_date);
    parameters["end_date"] = display_date(options.end_date);
    parameters["available_only"] = options.available_only;

    Json units = Json::object();
    units["balances_and_flows"] = "100 million CNY unless field says 10b";
    units["rates_and_returns"] = "percentage-points";
    units["csi300_close"] = "index-points";

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["normalized"] = static_cast<std::uint64_t>(normalized.size());
    counts["signal_available"] = static_cast<std::uint64_t>(available);
    counts["upstream_placeholder"] = static_cast<std::uint64_t>(placeholders);
    counts["available_only_excluded"] = static_cast<std::uint64_t>(excluded);
    counts["eligible"] = static_cast<std::uint64_t>(eligible);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;

    Json cache = Json::object();
    cache["hit"] = false; cache["stale"] = false; cache["age_seconds"] = 0;
    cache["ttl_seconds"] = options.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-flow-followup-native-v2";
    result["availability"] = "live";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["available_views"] = available_views();
    result["parameters"] = std::move(parameters);
    result["methodology"] = std::move(methodology);
    result["units"] = std::move(units);
    result["warnings"] = std::move(warnings);
    result["summary"] = std::move(summary);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["source"] = source_document(upstream, raw_rows, request_id);
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    cache_[key] = CachedDocument{result, std::time(nullptr)};
    return result;
}


}  // namespace tdx

