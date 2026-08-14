#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <cstdint>

namespace tdx::recon_contract_detail {

void validate_benchmark_analysis_contract(const Json& document, const Json&,
                                          Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-benchmark-analysis-native-v1"),
                  "tdx-market-benchmark-analysis-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* stocks = member_path(document, {"summary", "stocks"});
    const auto* stages = member_path(document, {"summary", "stages"});
    const bool population = string_is(member(document, "view"), "stocks") &&
        string_is(member(document, "stage"), "216") && stocks &&
        stocks->is_number() && stocks->as_number() >= 5000 && stages &&
        stages->is_number() && stages->as_number() == 13;
    add_assertion(result, "latest_stage_population", population,
                  "stage 216 contains at least 5000 stocks and model declares 13 stages",
                  population);
    bool stage_semantics = false, raw_audit = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() &&
            string_is(member(row, "source_resource"), "list/func_jzfx216_1.jsn");
        if (!raw || !raw->is_object()) continue;
        const auto source = numeric_value(member(*raw, "zaf1"));
        const auto normalized = numeric_value(member(row, "security_return_pct"));
        stage_semantics = stage_semantics || (string_is(member(row, "stage_id"), "216") &&
            string_is(member(row, "stage_start_date"), "20240918") &&
            member(row, "stage_open") && member(row, "stage_open")->is_bool() &&
            member(row, "stage_open")->as_bool() && source && normalized &&
            std::abs(*source - *normalized) < 0.000001);
    } else raw_audit = false;
    add_assertion(result, "stage_semantics", stage_semantics,
                  "stage 216 starts 20240918, remains open and preserves source percent values",
                  stage_semantics);
    add_assertion(result, "raw_audit", raw_audit,
                  "lazy stage projection reads only JZFX216 and retains raw rows", raw_audit);
    const auto* sources = member(document, "sources");
    const bool exact_source = sources && sources->is_array() &&
        sources->size() == 1 && source_exists(document, "list/func_jzfx216_1.jsn");
    add_assertion(result, "lazy_source_boundary", exact_source,
                  "one stage request loads one JSN resource", exact_source);
}

void validate_consensus_stage_rankings_contract(const Json& document, const Json&,
                                                Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-consensus-native-v1"),
                  "tdx-market-consensus-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "mode",
                  string_is(member(document, "mode"), "master"),
                  "master", value_or_null(member(document, "mode")));
    add_assertion(result, "category",
                  string_is(member(document, "category"), "year-low-rise"),
                  "year-low-rise", value_or_null(member(document, "category")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") ||
                      string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* categories = member(document, "categories");
    add_assertion(result, "nine_categories",
                  categories && categories->is_array() && categories->size() == 9,
                  9, categories && categories->is_array()
                      ? Json(static_cast<std::uint64_t>(categories->size())) : Json(nullptr));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array().front() : nullptr;
    const bool stage_fields = first &&
        numeric_value(member(*first, "year_low_price")) &&
        numeric_value(member(*first, "latest_close")) &&
        numeric_value(member(*first, "change_from_year_low_pct"));
    add_assertion(result, "typed_stage_fields", stage_fields,
                  "year low, source close, and source percent",
                  first ? *first : Json(nullptr));
    const bool exact_source = source_exists(
        document, "list/func_yzyq109_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_yzyq109_1.jsn", exact_source);
}

}  // namespace tdx::recon_contract_detail
