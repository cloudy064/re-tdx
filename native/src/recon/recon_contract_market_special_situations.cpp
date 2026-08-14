#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

void validate_market_special_situations_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-special-situations-native-v1"),
                  "tdx-market-special-situations-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "all") ||
                      string_is(member(document, "view"), "legacy"),
                  "all or legacy", value_or_null(member(document, "view")));
    add_assertion(result, "offline_quote_boundary",
                  string_is(member(document, "mode"), "catalog") &&
                      member(document, "quote_source") &&
                      member(document, "quote_source")->is_null() &&
                      member(document, "quote_errors") &&
                      member(document, "quote_errors")->is_array() &&
                      member(document, "quote_errors")->as_array().empty(),
                  "include_quotes=0 keeps the master table independent of L1",
                  value_or_null(member(document, "mode")));
    const auto* matched = member(document, "match_count");
    const auto* mergers = member_path(document, {"summary", "mergers"});
    const auto* b_to_h = member_path(document, {"summary", "b_to_h"});
    const auto* risks = member_path(document, {"summary", "market_cap_warnings"});
    const auto* unique_risks = member_path(
        document, {"summary", "market_cap_unique_securities"});
    const auto* twenty_day = member_path(
        document, {"summary", "twenty_day_only"});
    const auto* one_year = member_path(document, {"summary", "one_year_only"});
    const auto* both = member_path(document, {"summary", "both_triggers"});
    const bool family_shape = matched && matched->is_number() &&
        mergers && mergers->is_number() && b_to_h && b_to_h->is_number() &&
        risks && risks->is_number();
    const bool family_reconciliation = family_shape &&
        mergers->as_number() + b_to_h->as_number() + risks->as_number() ==
            matched->as_number();
    const bool population = family_reconciliation && mergers->as_number() >= 1 &&
        b_to_h->as_number() >= 1 && risks->as_number() >= 100 &&
        unique_risks && unique_risks->is_number() &&
        unique_risks->as_number() >= 80 &&
        unique_risks->as_number() <= risks->as_number();
    add_assertion(result, "population", population,
                  "all legacy families remain populated and warning identities are bounded",
                  population);
    add_assertion(result, "family_reconciliation", family_reconciliation,
                  "three family counts equal match_count", family_reconciliation);
    const bool trigger_reconciliation = twenty_day && twenty_day->is_number() &&
        one_year && one_year->is_number() && both && both->is_number() &&
        twenty_day->as_number() + one_year->as_number() + both->as_number() ==
            risks->as_number();
    add_assertion(result, "trigger_reconciliation", trigger_reconciliation,
                  "20-session, one-year and dual triggers equal warning rows",
                  trigger_reconciliation);

    const auto* records = member(document, "records");
    bool recovered_bj = false;
    bool normalized_merger = false;
    bool normalized_b_to_h = false;
    bool normalized_risk = false;
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* primary = member(row, "primary_security");
            const auto* related = member(row, "related_security");
            const bool auditable = primary && primary->is_object() &&
                member(row, "raw") && member(row, "raw")->is_object() &&
                member(row, "source_resource") &&
                member(row, "source_resource")->is_string();
            if (string_is(member(row, "kind"), "merger")) {
                normalized_merger = normalized_merger ||
                    (auditable && related && related->is_object() &&
                     member(row, "absorber_exchange_price") &&
                     member(row, "absorber_exchange_price")->is_number() &&
                     member(row, "absorbed_cash_option_price") &&
                     member(row, "absorbed_cash_option_price")->is_number());
                recovered_bj = recovered_bj ||
                    (related && related->is_object() &&
                     string_is(member(*related, "market"), "bj") &&
                     string_is(member(*related, "code"), "834082") &&
                     string_is(member(*related, "security_id"), "BJ834082"));
            } else if (string_is(member(row, "kind"), "b-to-h")) {
                normalized_b_to_h = normalized_b_to_h ||
                    (auditable && related && related->is_object() &&
                     member(row, "cash_option_price") &&
                     member(row, "cash_option_price")->is_number() &&
                     member(row, "currency") && member(row, "currency")->is_string());
            } else if (string_is(member(row, "kind"), "market-cap-risk")) {
                normalized_risk = normalized_risk ||
                    (auditable && member(row, "twenty_day_triggered") &&
                     member(row, "twenty_day_triggered")->is_bool() &&
                     member(row, "one_year_triggered") &&
                     member(row, "one_year_triggered")->is_bool() &&
                     member(row, "sample_indexes") &&
                     member(row, "sample_indexes")->is_array());
            }
        }
    }
    add_assertion(result, "normalized_families",
                  normalized_merger && normalized_b_to_h && normalized_risk,
                  "auditable normalized row from every family",
                  normalized_merger && normalized_b_to_h && normalized_risk);
    add_assertion(result, "blank_market_recovered",
                  recovered_bj, "blank $SC1 recovered as BJ834082", recovered_bj);
    const std::vector<std::string> required_sources{
        "list/func_agtl101_1.jsn", "list/func_agtl102_1.jsn",
        "list/func_cdgc101_1.jsn"};
    bool all_sources = true;
    for (const auto& source : required_sources)
        all_sources = all_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", all_sources,
                  static_cast<std::uint64_t>(required_sources.size()), all_sources);
}

}  // namespace tdx::recon_contract_detail
