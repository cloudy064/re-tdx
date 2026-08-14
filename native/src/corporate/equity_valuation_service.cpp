#include "equity_valuation_internal.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/pbrpc.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

using detail::equity_valuation::ViewKind;
using detail::equity_valuation::ViewSpec;

Json selection_document(const EquityValuationQuery& options, const ViewSpec& view,
                        const BlockData& blocks) {
    using namespace detail::equity_valuation;
    Json selection = Json::object();
    selection["security"] = Json(nullptr);
    selection["industry"] = Json(nullptr);
    if (view.history)
        selection["security"] = security_identity(options.market, options.code, {}, blocks);
    if (view.industry_members)
        selection["industry"] = industry_identity(
            options.industry_code, options.industry_market, {}, blocks);
    return selection;
}

Json parameter_document(const EquityValuationQuery& options, const ViewSpec& view) {
    using namespace detail::equity_valuation;
    Json parameters = Json::object();
    parameters["start_date"] = view.pe && view.kind != ViewKind::pe_industries
        ? Json(display_date(options.start_date)) : Json(nullptr);
    parameters["end_date"] = view.kind == ViewKind::pe_industries
        ? Json(nullptr) : Json(display_date(options.end_date));
    parameters["pe_type"] = view.pe ? Json(options.pe_type) : Json(nullptr);
    parameters["required_return_rate_pct"] = view.kind == ViewKind::pe_industry_members
        ? Json(options.required_return_rate_pct) : Json(nullptr);
    return parameters;
}

Json summary_document(const Json& records, const ViewSpec& view) {
    using detail::equity_valuation::text;
    Json summary = Json::object();
    if (view.history) {
        summary["first"] = records.size() ? records.as_array().front() : Json(nullptr);
        summary["latest"] = records.size() ? records.as_array().back() : Json(nullptr);
    } else if (view.industry_members) {
        Json counts = Json::object();
        counts["overvalued"] = 0;
        counts["undervalued"] = 0;
        counts["reasonable"] = 0;
        counts["unknown"] = 0;
        for (const auto& record : records.as_array()) {
            const auto& value = view.kind == ViewKind::pb_roe_members
                ? record.at("valuation_judgment")
                : record.at("current_year").at("valuation_judgment");
            const auto label = text(value, "label");
            const auto key = label.empty() ? "unknown" : label;
            counts[key] = static_cast<std::uint64_t>(counts.at(key).as_number() + 1);
        }
        summary["valuation_counts"] = std::move(counts);
    }
    return summary;
}

Json unit_document() {
    Json units = Json::object();
    units["pe"] = "multiple";
    units["pb"] = "multiple";
    units["roe_pct"] = "percentage-points";
    units["required_return_rate_pct"] = "percentage-points";
    return units;
}

}  // namespace

EquityValuationService::EquityValuationService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json EquityValuationService::query(const EquityValuationQuery& input) {
    using namespace detail::equity_valuation;
    const auto& view = view_spec(input.view);
    int pe_type = 0;
    const auto options = normalize_query_options(input, view, pe_type);
    const auto key = cache_key(options);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!options.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - cached->second.fetched_at));
        if (age < options.cache_ttl_seconds) {
            auto result = cached->second.document;
            result["cache"]["hit"] = true;
            result["cache"]["age_seconds"] = age;
            return result;
        }
    }

    const auto plan = build_request_plan(options, view, pe_type);
    Json upstream;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            upstream = execute_pbrpc_config(
                root_, plan.request_id, plan.replacements, {}, {}, {}, plan.source_file, {},
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
            std::this_thread::sleep_for(
                std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    auto records = normalize_equity_valuation_rows(view.id, raw_rows, blocks_);
    const auto normalized_count = records.size();
    const bool truncated = records.size() > static_cast<std::size_t>(options.limit);
    if (truncated && view.history) {
        records.as_array().erase(records.as_array().begin(),
            records.as_array().end() - static_cast<std::ptrdiff_t>(options.limit));
    } else if (truncated) {
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    }

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["normalized"] = static_cast<std::uint64_t>(normalized_count);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = options.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-equity-valuation-native-v1";
    result["availability"] = "live";
    result["generated_at"] = now_text();
    result["view"] = view.id;
    result["selection"] = selection_document(options, view, blocks_);
    result["parameters"] = parameter_document(options, view);
    result["methodology"] = methodology(view);
    result["units"] = unit_document();
    result["summary"] = summary_document(records, view);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["source"] = source_document(upstream, plan.request_id);
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    cache_[key] = CachedDocument{result, std::time(nullptr)};
    return result;
}

}  // namespace tdx
