#include "recon_contract_market_data_bonds_internal.hpp"

#include "recon_contract_internal.hpp"

#include <cmath>

namespace tdx::recon_contract_detail {

void validate_bond_reference_aa_plus(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-bond-reference-native-v1"),
                  "tdx-market-bond-reference-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") || string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* rows = member_path(document, {"summary", "source_row_count"});
    const auto* matched = member(document, "match_count");
    const auto* returned = member(document, "returned");
    const bool population = rows && rows->is_number() && rows->as_number() >= 900 &&
        matched && matched->is_number() &&
        matched->as_number() == rows->as_number() &&
        returned && returned->is_number() && returned->as_number() == 5 &&
        string_is(member_path(document, {"summary", "source_group"}), "rating") &&
        string_is(member_path(document, {"summary", "source_bucket"}), "aa-plus");
    add_assertion(result, "population", population,
                  "AA+ source and matched counts reconcile at 900+ rows and return five",
                  population);
    bool terms = true;
    const auto* records = member(document, "records");
    if (!records || !records->is_array() || records->size() != 5) terms = false;
    else for (const auto& row : records->as_array()) {
        const auto* schedule = member(row, "coupon_schedule");
        const auto* remaining = member(row, "remaining_coupon_schedule");
        terms = terms && string_is(member(row, "source_resource"), "list/zq_aaj201.jsn") &&
            string_is(member(row, "bond_credit_rating"), "AA+") &&
            member_path(row, {"security", "code"}) && member_path(row, {"security", "name"}) &&
            member(row, "maturity_date") && member(row, "current_coupon_rate_pct") &&
            schedule && schedule->is_array() && schedule->size() > 0 &&
            remaining && remaining->is_array() && member(row, "raw") &&
            member(row, "raw")->is_object();
    }
    add_assertion(result, "terms_and_schedules", terms,
                  "five AA+ bonds retain terms, schedules and raw rows", terms);
    const bool source = source_exists(document, "list/zq_aaj201.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 1;
    add_assertion(result, "exact_source", source, "list/zq_aaj201.jsn", source);
}

void validate_bond_reference_government(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-bond-reference-native-v1"),
                  "tdx-market-bond-reference-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") || string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* rows = member_path(document, {"summary", "source_row_count"});
    const auto* matched = member(document, "match_count");
    const auto* returned = member(document, "returned");
    const bool population = rows && rows->is_number() && rows->as_number() >= 400 &&
        matched && matched->is_number() && matched->as_number() >= 400 &&
        returned && returned->is_number() && returned->as_number() == 5 &&
        string_is(member_path(document, {"summary", "source_group"}), "category") &&
        string_is(member_path(document, {"summary", "source_bucket"}), "government");
    add_assertion(result, "population", population,
                  "government category has at least 400 rows and returns five", population);
    bool terms = true;
    const auto* records = member(document, "records");
    if (!records || !records->is_array() || records->size() != 5) terms = false;
    else for (const auto& row : records->as_array()) {
        const auto* schedule = member(row, "coupon_schedule");
        terms = terms && string_is(member(row, "source_resource"), "list/zqgz201.jsn") &&
            member_path(row, {"security", "code"}) && member_path(row, {"security", "name"}) &&
            member(row, "maturity_date") && member(row, "current_coupon_rate_pct") &&
            string_is(member(row, "source_scale_semantics"), "client-master-hidden-unit") &&
            member(row, "issue_size_yuan") && member(row, "issue_size_yuan")->is_null() &&
            member(row, "underlying") && member(row, "underlying")->is_null() &&
            schedule && schedule->is_array() && schedule->size() > 0 &&
            member(row, "raw") && member(row, "raw")->is_object();
    }
    add_assertion(result, "terms_schedules_and_scale_boundary", terms,
                  "five government bonds retain terms/schedules without guessing hidden scale",
                  terms);
    const bool source = source_exists(document, "list/zqgz201.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 1;
    add_assertion(result, "exact_source", source, "list/zqgz201.jsn", source);
}

void validate_bond_reference_policy_financial(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-bond-reference-native-v1"),
                  "tdx-market-bond-reference-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* matched = member(document, "match_count");
    const auto* returned = member(document, "returned");
    add_assertion(result, "population",
                  string_is(member_path(document, {"summary", "source_group"}), "category") &&
                      string_is(member_path(document, {"summary", "source_bucket"}),
                                "policy-financial") &&
                      matched && matched->is_number() && matched->as_number() >= 1 &&
                      returned && returned->is_number() &&
                      returned->as_number() == matched->as_number(),
                  "non-empty complete policy-financial category", value_or_null(matched));
    const auto* reconciliation = member(document, "projection_reconciliation");
    const auto master_count = reconciliation
        ? numeric_value(member(*reconciliation, "master_count")) : std::nullopt;
    const auto union_count = reconciliation
        ? numeric_value(member(*reconciliation, "projection_union_count")) : std::nullopt;
    add_assertion(result, "exact_projection_reconciliation",
                  reconciliation && reconciliation->is_object() &&
                      bool_is(member(*reconciliation, "exact_match"), true) &&
                      bool_is(member(*reconciliation, "counts_match"), true) &&
                      master_count && union_count && *master_count == *union_count &&
                      member(*reconciliation, "projections") &&
                      member(*reconciliation, "projections")->is_array() &&
                      member(*reconciliation, "projections")->size() == 2,
                  "master exactly equals SH+SZ projection union", value_or_null(reconciliation));
    const auto* records = member(document, "records");
    bool units = records && records->is_array() && !records->as_array().empty();
    if (units) for (const auto& record : records->as_array()) {
        const auto source_100m = numeric_value(member(record, "issue_size_source_100m"));
        const auto yuan = numeric_value(member(record, "issue_size_yuan"));
        units = units && source_100m && yuan && *source_100m > 0 &&
            std::abs(*yuan - *source_100m * 100000000.0) < 0.01 &&
            member_path(record, {"security", "code"}) &&
            member(record, "raw") && member(record, "raw")->is_object();
    }
    add_assertion(result, "issue_size_units", units,
                  "GM hundred-million yuan scaled to base yuan", units);
    const bool exact_sources = source_exists(document, "list/zqjrz201.jsn") &&
        source_exists(document, "list/zq_jrz201_1.jsn") &&
        source_exists(document, "list/zq_jrz201_2.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 3;
    add_assertion(result, "exact_sources", exact_sources,
                  "policy master plus SH/SZ projections", exact_sources);
}

}  // namespace tdx::recon_contract_detail
