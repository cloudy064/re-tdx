#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <cstdint>

namespace tdx::recon_contract_detail {

void validate_forecast_latest_contract(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-forecasts-native-v1"),
                  "tdx-market-forecasts-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "latest"),
                  "latest", value_or_null(member(document, "view")));
    const auto* summary = member(document, "summary");
    const auto* latest = summary ? member(*summary, "latest_forecasts") : nullptr;
    const auto* current = summary ? member(*summary, "current_report_rows") : nullptr;
    const auto* expected = summary ? member(*summary, "current_report_expected") : nullptr;
    const auto* future = summary ? member(*summary, "future_report_rows") : nullptr;
    const auto* gap = summary ? member(*summary, "current_report_gap") : nullptr;
    add_assertion(result, "latest_population_and_reconciliation",
                  latest && latest->is_number() && latest->as_number() >= 1700 &&
                      current && current->is_number() && current->as_number() >= 1700 &&
                      expected && expected->is_number() &&
                          expected->as_number() >= current->as_number() &&
                      future && future->is_number() && future->as_number() >= 1 &&
                      gap && gap->is_number() &&
                          std::abs(gap->as_number() - expected->as_number() +
                                   current->as_number()) < 0.000001,
                  "large current table, future periods, and exact master/detail gap",
                  summary ? *summary : Json(nullptr));
    const auto* rows = member(document, "securities");
    const Json* first = rows && rows->is_array() && rows->size() == 5
        ? &rows->as_array().front() : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "typed_forecast_row",
                  first && security && member(*security, "code") &&
                      member(*first, "forecast_date") &&
                      member(*first, "report_period") &&
                      member(*first, "forecast_type") &&
                      member(*first, "profit_lower_yuan") &&
                      member(*first, "growth_lower_pct") &&
                      string_is(member(*first, "source_variant"),
                                "all-market-static") && raw && raw->is_object(),
                  "security/date/period/type/profit/growth/static-source/raw",
                  first ? *first : Json(nullptr));
    const auto* sources = member(document, "sources");
    bool exact_source = false;
    if (sources && sources->is_array())
        for (const auto& source : sources->as_array())
            if (string_is(member(source, "resource"),
                          "list/func_cbpl101_1.jsn")) exact_source = true;
    add_assertion(result, "forecast_sources",
                  sources && sources->is_array() && sources->size() == 3 && exact_source,
                  "three sources including list/func_cbpl101_1.jsn",
                  sources ? Json(static_cast<std::uint64_t>(sources->size())) : Json(nullptr));
}

}  // namespace tdx::recon_contract_detail
