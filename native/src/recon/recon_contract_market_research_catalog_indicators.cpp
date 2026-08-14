#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <set>
#include <string>

namespace tdx::recon_contract_detail {
namespace {

// Shared preamble for the catalog and detail views.  Returns the "sources"
// member so each view can apply its own resource expectations.
const Json* assert_indicator_common(const Json& document, Json& result,
                                    bool detail_mode) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-economic-indicators-native-v1"),
                  "tdx-market-economic-indicators-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"),
                            detail_mode ? "indicator" : "catalog"),
                  detail_mode ? "indicator" : "catalog",
                  value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* total = member_path(document, {"summary", "indicator_count"});
    add_assertion(result, "indicator_count",
                  total && total->is_number() && total->as_number() == 72,
                  72, value_or_null(total));
    return member(document, "sources");
}

}  // namespace

void validate_economic_indicators_catalog_contract(const Json& document,
                                                   Json& result) {
    const auto* sources = assert_indicator_common(document, result, false);
    const auto* rows = member(document, "indicators");
    const Json* first = rows && rows->is_array() && rows->size() >= 2
        ? &rows->as_array()[0] : nullptr;
    const Json* second = rows && rows->is_array() && rows->size() >= 2
        ? &rows->as_array()[1] : nullptr;
    const auto* first_date = first ? member(*first, "update_date") : nullptr;
    const auto* second_date = second ? member(*second, "update_date") : nullptr;
    add_assertion(result, "records_present", first && second,
                  "at least two indicators", rows ? Json(rows->size()) : Json(nullptr));
    add_assertion(result, "descending_update_date",
                  first_date && second_date && first_date->is_string() &&
                      second_date->is_string() &&
                      first_date->as_string() >= second_date->as_string(),
                  true, first_date && second_date ? Json(true) : Json(false));
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "catalog_semantics",
                  first && member(*first, "current_value") &&
                      member(*first, "month_on_month_pct") &&
                      member(*first, "year_on_year_pct") &&
                      raw && raw->is_object(),
                  "value/mom/yoy/raw", first ? *first : Json(nullptr));
    bool exact = false;
    if (sources && sources->is_array() && sources->size() == 1)
        exact = string_is(member(sources->as_array()[0], "resource"),
                          "list/func_jjzb101_1.jsn");
    add_assertion(result, "catalog_source", exact,
                  "list/func_jjzb101_1.jsn", value_or_null(sources));
}

void validate_economic_indicator_detail_contract(const Json& document,
                                                 Json& result) {
    const auto* sources = assert_indicator_common(document, result, true);
    const auto* selected = member(document, "selected_indicator");
    add_assertion(result, "selected_indicator",
                  selected &&
                      string_is(member(*selected, "indicator_id"), "M2800000005") &&
                      string_is(member(*selected, "unit"), "元/吨") &&
                      member(*selected, "current_value") &&
                      member(*selected, "month_on_month_pct") &&
                      member(*selected, "year_on_year_pct"),
                  "M2800000005 with current value and changes",
                  value_or_null(selected));
    const auto* history = member(document, "history");
    bool history_sorted = history && history->is_array() && history->size() >= 90;
    if (history_sorted) {
        const auto& values = history->as_array();
        for (std::size_t i = 1; i < values.size(); ++i)
            if (values[i - 1].at("date").as_string() >
                values[i].at("date").as_string()) history_sorted = false;
    }
    add_assertion(result, "history_series", history_sorted,
                  "at least 90 points sorted ascending",
                  history ? Json(history->size()) : Json(nullptr));
    const auto* related_total = member_path(
        document, {"summary", "related_security_count"});
    const auto* related = member(document, "related_securities");
    add_assertion(result, "related_coverage",
                  related_total && related_total->is_number() &&
                      related_total->as_number() >= 100 && related &&
                      related->is_array() && related->size() == 100,
                  "at least 100 total and 100 returned",
                  related_total ? *related_total : Json(nullptr));
    std::size_t names = 0, quotes = 0, raw_rows = 0;
    if (related && related->is_array()) {
        for (const auto& row : related->as_array()) {
            const auto* security = member(row, "security");
            if (security && bool_is(member(*security, "name_resolved"), true)) ++names;
            if (bool_is(member(row, "quote_available"), true)) ++quotes;
            const auto* raw = member(row, "raw");
            if (raw && raw->is_object()) ++raw_rows;
        }
    }
    add_assertion(result, "related_semantics",
                  names >= 95 && quotes == 100 && raw_rows == 100,
                  "at least 95 names and all 100 quotes/raw rows",
                  Json(static_cast<std::uint64_t>(names)));
    const auto* quote_received = member_path(document, {"quote_source", "received"});
    add_assertion(result, "quote_source",
                  quote_received && quote_received->is_number() &&
                      quote_received->as_number() == 100,
                  100, value_or_null(quote_received));
    std::set<std::string> actual;
    if (sources && sources->is_array())
        for (const auto& source : sources->as_array()) {
            const auto* resource = member(source, "resource");
            if (resource && resource->is_string()) actual.insert(resource->as_string());
        }
    const std::set<std::string> expected{
        "list/func_jjzb101_1.jsn", "jjzb1/M2800000005.jsn",
        "jjzb2/M2800000005.jsn"};
    add_assertion(result, "detail_sources", actual == expected,
                  "three exact JJZB resources",
                  Json(static_cast<std::uint64_t>(actual.size())));
}

}  // namespace tdx::recon_contract_detail
