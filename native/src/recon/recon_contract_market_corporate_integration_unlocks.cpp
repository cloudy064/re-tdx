#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>

namespace tdx::recon_contract_detail {

void validate_recent_large_unlocks_contract(const Json& document, const Json&,
                                           Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-unlocks-native-v1"),
                  "tdx-market-unlocks-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "recent-large"),
                  "recent-large", value_or_null(member(document, "view")));
    add_assertion(result, "mode",
                  string_is(member(document, "mode"), "recent-large"),
                  "recent-large", value_or_null(member(document, "mode")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") ||
                      string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* events_count = member_path(document, {"summary", "events"});
    const auto* securities = member_path(document, {"summary", "unique_securities"});
    const auto* implemented = member_path(document, {"summary", "implemented_events"});
    const auto* pending = member_path(document, {"summary", "pending_events"});
    const bool population = events_count && events_count->is_number() &&
        events_count->as_number() >= 30 && securities && securities->is_number() &&
        securities->as_number() >= 30 && implemented && implemented->is_number() &&
        pending && pending->is_number() &&
        implemented->as_number() + pending->as_number() == events_count->as_number();
    add_assertion(result, "rolling_window_population", population,
                  "at least 30 events with complete time-derived status counts", population);
    const auto* events = member(document, "events");
    const Json* first = events && events->is_array() && events->size() >= 2
        ? &events->as_array()[0] : nullptr;
    const Json* second = events && events->is_array() && events->size() >= 2
        ? &events->as_array()[1] : nullptr;
    const auto* first_date = first ? member(*first, "date") : nullptr;
    const auto* second_date = second ? member(*second, "date") : nullptr;
    add_assertion(result, "descending_date_sort",
                  first_date && first_date->is_string() && first_date->as_string().size() == 8 &&
                      second_date && second_date->is_string() &&
                      first_date->as_string() >= second_date->as_string(),
                  true, first_date && second_date ?
                      Json(first_date->as_string() >= second_date->as_string()) : Json(nullptr));
    const auto* ratio = first ? member(*first, "unlock_to_total_ratio") : nullptr;
    const auto* percent = first ? member(*first, "unlock_to_total_pct") : nullptr;
    const bool ratio_projection = ratio && ratio->is_number() && percent &&
        percent->is_number() &&
        std::abs(percent->as_number() - ratio->as_number() * 100.0) < 0.000001;
    add_assertion(result, "ratio_semantics",
                  first && string_is(member(*first, "source_kind"),
                                     "recent-large-window") &&
                      member(*first, "security") && member(*first, "security")->is_object() &&
                      member(*first, "raw") && member(*first, "raw")->is_object() &&
                      ratio_projection,
                  "raw jjgzb ratio projected to percentage points", value_or_null(first));
    add_assertion(result, "no_fake_details",
                  array_empty(member(document, "details")) &&
                      array_empty(member(document, "detail_errors")),
                  true, Json(array_empty(member(document, "details")) &&
                             array_empty(member(document, "detail_errors"))));
    const bool exact_source = source_exists(document, "list/func_jqgz103_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_jqgz103_1.jsn", exact_source);
}

void validate_unlock_monthly_pressure_contract(const Json& document, const Json&,
                                              Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-unlocks-native-v1"),
                  "tdx-market-unlocks-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_and_mode",
                  string_is(member(document, "view"), "monthly-pressure") &&
                      string_is(member(document, "mode"), "monthly-pressure"),
                  "monthly-pressure", value_or_null(member(document, "view")));
    const auto months_count = numeric_value(member_path(document, {"summary", "months"}));
    const auto formula_checked = numeric_value(
        member_path(document, {"summary", "formula_checked"}));
    const auto formula_mismatches = numeric_value(
        member_path(document, {"summary", "formula_mismatches"}));
    bool formula_ok = months_count && formula_checked && formula_mismatches &&
        *months_count >= 25 && *formula_checked == *months_count &&
        *formula_mismatches == 0;
    const auto* months = member(document, "months");
    if (months && months->is_array() && months->size() >= 25) {
        for (const auto& row : months->as_array()) {
            const auto raw_value = numeric_value(member_path(row, {"raw", "jjsz"}));
            const auto raw_shares = numeric_value(member_path(row, {"raw", "jjsl"}));
            const auto yuan = numeric_value(member(row, "unlock_market_value_yuan"));
            const auto shares = numeric_value(member(row, "unlock_shares"));
            const auto close_enough = [](double actual, double expected) {
                return std::abs(actual - expected) <=
                    std::max(0.01, std::abs(expected) * 1e-12);
            };
            formula_ok = formula_ok && raw_value && raw_shares && yuan && shares &&
                close_enough(*yuan, *raw_value * 100000000.0) &&
                close_enough(*shares, *raw_shares * 100000000.0) &&
                member(row, "month") && member(row, "raw") &&
                member(row, "raw")->is_object();
        }
    } else formula_ok = false;
    add_assertion(result, "monthly_units", formula_ok,
                  "25+ months and each hundred-million unit converts by x100000000",
                  formula_ok);
    add_assertion(result, "no_fake_event_details",
                  array_empty(member(document, "events")) &&
                      array_empty(member(document, "details")) &&
                      array_empty(member(document, "detail_errors")),
                  true, Json(array_empty(member(document, "events")) &&
                             array_empty(member(document, "details")) &&
                             array_empty(member(document, "detail_errors"))));
    const bool exact_source =
        source_exists(document, "list/func_dxfjj101_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_dxfjj101_1.jsn", exact_source);
}

}  // namespace tdx::recon_contract_detail
