#include "recon_contract_market_data_bonds_internal.hpp"

#include "recon_contract_internal.hpp"

#include <map>
#include <string>

namespace tdx::recon_contract_detail {

void validate_bond_reference_universe(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-bond-reference-native-v1"),
                  "tdx-market-bond-reference-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") ||
                      string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* rows = member_path(document, {"summary", "source_row_count"});
    const auto* matched = member(document, "match_count");
    const auto* returned = member(document, "returned");
    const bool population = rows && rows->is_number() && rows->as_number() >= 40000 &&
        matched && matched->is_number() && matched->as_number() >= 40000 &&
        returned && returned->is_number() && returned->as_number() == 5 &&
        string_is(member_path(document, {"summary", "source_group"}), "category") &&
        string_is(member_path(document, {"summary", "source_bucket"}), "all");
    add_assertion(result, "population", population,
                  "all-bond category has at least 40,000 rows and returns five",
                  population);
    bool terms = true, scheduled = false;
    const auto* records = member(document, "records");
    if (!records || !records->is_array() || records->size() != 5) terms = false;
    else for (const auto& row : records->as_array()) {
        const auto* schedule = member(row, "coupon_schedule");
        scheduled = scheduled || (schedule && schedule->is_array() && schedule->size() > 0);
        terms = terms && string_is(member(row, "source_resource"),
                                   "list/zq_zqqb201.jsn") &&
            nonempty_string(member_path(row, {"security", "security_id"})) &&
            nonempty_string(member_path(row, {"security", "code"})) &&
            nonempty_string(member_path(row, {"security", "name"})) &&
            member(row, "bond_type") && member(row, "maturity_date") &&
            member(row, "current_coupon_rate_pct") && schedule && schedule->is_array() &&
            member(row, "raw") && member(row, "raw")->is_object();
    }
    add_assertion(result, "terms_and_schedules", terms && scheduled,
                  "five bonds retain terms/raw rows and at least one coupon schedule",
                  terms && scheduled);
    const auto* sources = member(document, "sources");
    const auto* first_source = sources && sources->is_array() && sources->size()
        ? &sources->as_array().front() : nullptr;
    const bool source = source_exists(document, "list/zq_zqqb201.jsn") &&
        sources && sources->is_array() && sources->size() == 1 && first_source &&
        nonempty_string(member(*first_source, "endpoint"));
    add_assertion(result, "exact_source", source,
                  "list/zq_zqqb201.jsn with a concrete local or remote endpoint", source);
}

void validate_bond_reference_corporate(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-bond-reference-native-v1"),
                  "tdx-market-bond-reference-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* rows = member_path(document, {"summary", "source_row_count"});
    const auto* unique = member_path(document, {"summary", "matched_unique_security_count"});
    const auto* matched = member(document, "match_count");
    const auto* returned = member(document, "returned");
    const bool population = rows && rows->is_number() && rows->as_number() >= 7000 &&
        unique && unique->is_number() && unique->as_number() == rows->as_number() &&
        matched && matched->is_number() && matched->as_number() == rows->as_number() &&
        returned && returned->is_number() && returned->as_number() == 5 &&
        string_is(member_path(document, {"summary", "source_bucket"}), "corporate");
    add_assertion(result, "population", population,
                  "corporate master has 7,000+ unique reconciled rows and returns five",
                  population);
    const auto* reconciliation = member(document, "projection_reconciliation");
    const auto* projections = reconciliation ? member(*reconciliation, "projections") : nullptr;
    bool projection_counts = projections && projections->is_array() && projections->size() == 2;
    double projection_sum = 0.0;
    if (projection_counts) {
        std::map<std::string, double> counts;
        for (const auto& projection : projections->as_array()) {
            const auto* market = member(projection, "market");
            const auto* count = member(projection, "count");
            if (market && market->is_string() && count && count->is_number()) {
                counts[market->as_string()] = count->as_number();
                projection_sum += count->as_number();
            }
        }
        projection_counts = counts.count("sh") == 1 && counts.count("sz") == 1 &&
            counts["sh"] > 0 && counts["sz"] > 0;
    }
    const auto master_count = reconciliation
        ? numeric_value(member(*reconciliation, "master_count")) : std::nullopt;
    const auto projection_union = reconciliation
        ? numeric_value(member(*reconciliation, "projection_union_count")) : std::nullopt;
    const auto* master_only = reconciliation ? member(*reconciliation, "master_only") : nullptr;
    const auto* projection_only = reconciliation
        ? member(*reconciliation, "projection_only") : nullptr;
    const bool exact_match = reconciliation &&
        bool_is(member(*reconciliation, "exact_match"), true);
    const bool explicit_difference = reconciliation &&
        bool_is(member(*reconciliation, "exact_match"), false) &&
        master_only && master_only->is_array() && !master_only->as_array().empty() &&
        projection_only && projection_only->is_array() && !projection_only->as_array().empty() &&
        master_count && projection_union &&
        *master_count - static_cast<double>(master_only->size()) ==
            *projection_union - static_cast<double>(projection_only->size());
    const bool exact = reconciliation && master_count && projection_union &&
        string_is(member(*reconciliation, "master_resource"), "list/zqgsz201.jsn") &&
        unique && *master_count == unique->as_number() && projection_counts &&
        projection_sum == *projection_union &&
        ((exact_match && *master_count == *projection_union) || explicit_difference);
    add_assertion(result, "projection_reconciliation", exact,
                  "Shanghai/Shenzhen projection counts and explicit set differences reconcile with the client master",
                  exact);
    bool terms = true;
    const auto* records = member(document, "records");
    if (!records || !records->is_array() || records->size() != 5) terms = false;
    else for (const auto& row : records->as_array())
        terms = terms && string_is(member(row, "source_resource"), "list/zqgsz201.jsn") &&
            nonempty_string(member_path(row, {"security", "security_id"})) &&
            nonempty_string(member(row, "client_instrument_id")) &&
            member(row, "source_scale_raw") &&
            string_is(member(row, "source_scale_semantics"), "client-master-hidden-unit") &&
            member(row, "raw") && member(row, "raw")->is_object();
    add_assertion(result, "master_aliases_and_terms", terms,
                  "client quote aliases, explicit hidden scale semantics and raw rows", terms);
    const bool sources = source_exists(document, "list/zqgsz201.jsn") &&
        source_exists(document, "list/zq_gsz201_1.jsn") &&
        source_exists(document, "list/zq_gsz201_2.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 3;
    add_assertion(result, "exact_sources", sources,
                  "corporate master plus Shanghai/Shenzhen projections", sources);
}

void validate_bond_reference_private_boundary(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-bond-reference-native-v1"),
                  "tdx-market-bond-reference-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* rows = member_path(document, {"summary", "source_row_count"});
    const auto* unique = member_path(document, {"summary", "matched_unique_security_count"});
    const auto* reconciliation = member(document, "projection_reconciliation");
    const auto* master_only = reconciliation ? member(*reconciliation, "master_only") : nullptr;
    const auto* projection_only = reconciliation ? member(*reconciliation, "projection_only") : nullptr;
    const auto master_count = reconciliation
        ? numeric_value(member(*reconciliation, "master_count")) : std::nullopt;
    const auto master_rows = reconciliation
        ? numeric_value(member(*reconciliation, "master_row_count")) : std::nullopt;
    const auto projection_union = reconciliation
        ? numeric_value(member(*reconciliation, "projection_union_count")) : std::nullopt;
    const auto* projections = reconciliation ? member(*reconciliation, "projections") : nullptr;
    double projection_sum = 0.0;
    bool projection_shape = projections && projections->is_array() && projections->size() == 2;
    if (projection_shape)
        for (const auto& projection : projections->as_array()) {
            const auto count = numeric_value(member(projection, "count"));
            projection_shape = projection_shape && count && *count > 0.0 &&
                nonempty_string(member(projection, "resource"));
            if (count) projection_sum += *count;
        }
    const bool boundary = rows && rows->is_number() && rows->as_number() >= 11000 &&
        unique && unique->is_number() && unique->as_number() >= 11000 &&
        rows->as_number() >= unique->as_number() && reconciliation &&
        master_count && *master_count == unique->as_number() &&
        master_rows && *master_rows == rows->as_number() && projection_union &&
        projection_shape && projection_sum == *projection_union &&
        bool_is(member(*reconciliation, "exact_match"), false) &&
        master_only && master_only->is_array() && !master_only->as_array().empty() &&
        projection_only && projection_only->is_array() && !projection_only->as_array().empty() &&
        *master_count - static_cast<double>(master_only->size()) ==
            *projection_union - static_cast<double>(projection_only->size());
    add_assertion(result, "complete_vs_projections", boundary,
                  "11,000+ complete/projection sets reconcile through explicit bidirectional differences",
                  boundary);
    const auto* client = reconciliation
        ? member(*reconciliation, "client_master_comparison") : nullptr;
    const auto* matching = client ? member(*client, "matching_projection_resources") : nullptr;
    const auto client_count = client
        ? numeric_value(member(*client, "security_count")) : std::nullopt;
    const auto client_rows = client
        ? numeric_value(member(*client, "row_count")) : std::nullopt;
    double matching_projection_count = -1.0;
    if (projections && projections->is_array())
        for (const auto& projection : projections->as_array())
            if (string_is(member(projection, "resource"), "list/zq_smz201_2.jsn"))
                matching_projection_count =
                    numeric_value(member(projection, "count")).value_or(-1.0);
    const bool client_boundary = client && client_count && client_rows &&
        string_is(member(*client, "resource"), "list/zqsmz201.jsn") &&
        *client_count >= 1000 && *client_count == *client_rows &&
        *client_count == matching_projection_count &&
        bool_is(member(*client, "matches_single_market_projection"), true) &&
        matching && matching->is_array() && matching->size() == 1 &&
        string_is(&matching->as_array().front(), "list/zq_smz201_2.jsn");
    add_assertion(result, "client_master_boundary", client_boundary,
                  "client combined master dynamically equals the single Shenzhen projection",
                  client_boundary);
    const bool sources = source_exists(document, "list/gxjty_zq_smz101_1.jsn") &&
        source_exists(document, "list/zq_smz201_1.jsn") &&
        source_exists(document, "list/zq_smz201_2.jsn") &&
        source_exists(document, "list/zqsmz201.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 4;
    add_assertion(result, "exact_sources", sources,
                  "complete source, two market projections and client combined master", sources);
}

}  // namespace tdx::recon_contract_detail
