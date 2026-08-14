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

void validate_market_corporate_transitions_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-special-situations-native-v1"),
                  "tdx-market-special-situations-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_and_quote_boundary",
                  string_is(member(document, "view"), "all") &&
                      member(document, "quote_source") &&
                      member(document, "quote_source")->is_null(),
                  "all with include_quotes=0",
                  value_or_null(member(document, "view")));
    const auto* matched = member(document, "match_count");
    const auto* major_plans = member_path(
        document, {"summary", "major_restructuring_plans"});
    const auto* major_reviews = member_path(
        document, {"summary", "major_restructuring_reviews"});
    const auto* major_completed = member_path(
        document, {"summary", "major_restructuring_completed"});
    const auto* ordinary_plans = member_path(
        document, {"summary", "ordinary_merger_plans"});
    const auto* transfer_plans = member_path(
        document, {"summary", "neeq_transfer_plans"});
    const auto* regulations = member_path(
        document, {"summary", "neeq_regulation_events"});
    const auto* transferred = member_path(
        document, {"summary", "neeq_transfer_completed"});
    const auto* mergers = member_path(document, {"summary", "mergers"});
    const auto* b_to_h = member_path(document, {"summary", "b_to_h"});
    const auto* risks = member_path(document, {"summary", "market_cap_warnings"});
    const bool population = matched && matched->is_number() &&
        major_plans && major_plans->is_number() && major_plans->as_number() >= 300 &&
        major_reviews && major_reviews->is_number() && major_reviews->as_number() >= 10 &&
        major_completed && major_completed->is_number() && major_completed->as_number() >= 140 &&
        ordinary_plans && ordinary_plans->is_number() && ordinary_plans->as_number() >= 2400 &&
        transfer_plans && transfer_plans->is_number() && transfer_plans->as_number() >= 200 &&
        regulations && regulations->is_number() && regulations->as_number() >= 680 &&
        transferred && transferred->is_number() && transferred->as_number() >= 3 &&
        mergers && mergers->is_number() && mergers->as_number() >= 1 &&
        b_to_h && b_to_h->is_number() && b_to_h->as_number() >= 1 &&
        risks && risks->is_number() && risks->as_number() >= 100 &&
        major_plans->as_number() + major_reviews->as_number() +
            major_completed->as_number() + ordinary_plans->as_number() +
            transfer_plans->as_number() + regulations->as_number() +
            transferred->as_number() + mergers->as_number() +
            b_to_h->as_number() + risks->as_number() == matched->as_number();
    add_assertion(result, "expanded_population", population,
                  "seven expanded and three legacy families meet floors and exactly reconcile",
                  population);

    bool amount_units = false, transfer_finance = false;
    bool regulation_semantics = false, completed_semantics = false;
    const auto* records = member(document, "records");
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* kind_value = member(row, "kind");
            const auto kind = kind_value && kind_value->is_string()
                ? kind_value->as_string() : std::string{};
            if ((kind == "major-restructuring-plan" ||
                 kind == "major-restructuring-review" ||
                 kind == "major-restructuring-completed" ||
                 kind == "ordinary-merger-plan") &&
                member(row, "transaction_amount_yuan") &&
                member(row, "transaction_amount_yuan")->is_number() &&
                member(row, "transaction_amount_100m_yuan") &&
                member(row, "transaction_amount_100m_yuan")->is_number()) {
                const auto yuan = member(row, "transaction_amount_yuan")->as_number();
                const auto yi = member(row, "transaction_amount_100m_yuan")->as_number();
                amount_units = amount_units ||
                    std::abs(yuan / 100000000.0 - yi) < 0.000001;
            } else if (kind == "neeq-transfer-plan") {
                transfer_finance = transfer_finance ||
                    (member(row, "report_period") &&
                     member(row, "report_period")->is_string() &&
                     member(row, "net_assets_yuan") &&
                     member(row, "net_assets_yuan")->is_number() &&
                     member(row, "revenue_yuan") &&
                     member(row, "revenue_yuan")->is_number());
            } else if (kind == "neeq-regulation") {
                regulation_semantics = regulation_semantics ||
                    (member(row, "regulation_reason") &&
                     member(row, "regulation_reason")->is_string() &&
                     member(row, "regulation_measure") &&
                     member(row, "regulation_measure")->is_string());
            } else if (kind == "neeq-transfer-completed") {
                completed_semantics = completed_semantics ||
                    (member(row, "acceptance_date") &&
                     member(row, "acceptance_date")->is_string() &&
                     member(row, "listing_date") &&
                     member(row, "listing_date")->is_string() &&
                     member(row, "listing_venue_after") &&
                     member(row, "listing_venue_after")->is_string());
            }
        }
    }
    add_assertion(result, "corporate_amount_units", amount_units,
                  "yuan / 1e8 reconciles to 100m-yuan", amount_units);
    add_assertion(result, "neeq_typed_semantics",
                  transfer_finance && regulation_semantics && completed_semantics,
                  "transfer finance, self-regulation and completed venue fields",
                  transfer_finance && regulation_semantics && completed_semantics);
    const std::vector<std::string> required_sources{
        "list/func_qxfa105_1.jsn", "list/func_qxfa106_1.jsn",
        "list/func_qxfa110_1.jsn", "list/func_qxfa111_1.jsn",
        "list/func_xsbtj101_1.jsn", "list/func_xsbtj102_1.jsn",
        "list/func_yzb101_1.jsn"};
    bool all_sources = true;
    for (const auto& source : required_sources)
        all_sources = all_sources && source_exists(document, source);
    add_assertion(result, "exact_expanded_sources", all_sources,
                  static_cast<std::uint64_t>(required_sources.size()), all_sources);
}

}  // namespace tdx::recon_contract_detail
