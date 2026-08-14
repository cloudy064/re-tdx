#include "recon_contract_market_data_bonds_internal.hpp"

#include "recon_contract_internal.hpp"

#include <set>
#include <string>

namespace tdx::recon_contract_detail {

void validate_pending_convertible_bonds(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-convertible-bonds-native-v1"),
                  "tdx-market-convertible-bonds-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "pending"),
                  "pending", value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* summary = member(document, "summary");
    const auto* total = summary ? member(*summary, "pending_issues") : nullptr;
    add_assertion(result, "pending_issue_count",
                  total && total->is_number() && total->as_number() >= 100,
                  "number >= 100", value_or_null(total));
    const auto* records = member(document, "pending_issues");
    const Json* row = records && records->is_array() && !records->as_array().empty()
        ? &records->as_array().front() : nullptr;
    add_assertion(result, "record_present", row != nullptr, true, row != nullptr);
    const auto* underlying = row ? member(*row, "underlying") : nullptr;
    const auto* code = underlying ? member(*underlying, "code") : nullptr;
    add_assertion(result, "underlying_identity",
                  underlying && underlying->is_object() && code && code->is_string() &&
                      code->as_string().size() == 6,
                  "six-digit underlying security", value_or_null(underlying));
    const auto* raw = row ? member(*row, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && !sources->as_array().empty()
        ? &sources->as_array().front() : nullptr;
    const bool exact_source = source &&
        string_is(member(*source, "resource"), "list/dfkzz201_1.jsn");
    add_assertion(result, "pending_source", exact_source, "list/dfkzz201_1.jsn",
                  source ? value_or_null(member(*source, "resource")) : Json(nullptr));
    const auto* reconciliation = member(document, "projection_reconciliation");
    bool pending_projections = reconciliation && reconciliation->is_array() &&
        reconciliation->size() == 2;
    if (pending_projections) {
        std::set<std::string> projection_sources;
        for (const auto& item : reconciliation->as_array()) {
            const auto* common = member(item, "common_securities");
            const auto* projection_rows = member(item, "projection_rows");
            const auto* primary_only = member(item, "primary_only_security_ids");
            const auto* projection_only = member(item, "projection_only_security_ids");
            pending_projections = pending_projections &&
                common && common->is_number() && common->as_number() >= 130 &&
                projection_rows && projection_rows->is_number() &&
                projection_rows->as_number() >= 130 &&
                primary_only && primary_only->is_array() &&
                projection_only && projection_only->is_array() &&
                string_is(member(item, "primary_resource"), "list/dfkzz201_1.jsn");
            const auto* resource = member(item, "projection_resource");
            if (resource && resource->is_string())
                projection_sources.insert(resource->as_string());
        }
        pending_projections = pending_projections &&
            projection_sources.count("list/func_kzz102_1.jsn") &&
            projection_sources.count("list/gxjty_zq_dfkzz102_1.jsn");
    }
    add_assertion(result, "pending_projection_reconciliation", pending_projections,
                  "two typed client projections reconciled to dfkzz201", pending_projections);
    const bool exact_sources = sources && sources->is_array() && sources->size() == 3 &&
        source_exists(document, "list/dfkzz201_1.jsn") &&
        source_exists(document, "list/func_kzz102_1.jsn") &&
        source_exists(document, "list/gxjty_zq_dfkzz102_1.jsn");
    add_assertion(result, "pending_exact_sources", exact_sources,
                  "three pending sources", exact_sources);
    const auto* attempts = source ? member(*source, "attempts") : nullptr;
    const auto* stale = source ? member(*source, "stale") : nullptr;
    add_assertion(result, "source_health",
                  attempts && attempts->is_number() && attempts->as_number() >= 1 &&
                      stale && stale->is_bool(), true,
                  attempts && stale ? Json(true) : Json(false));
}

}  // namespace tdx::recon_contract_detail
