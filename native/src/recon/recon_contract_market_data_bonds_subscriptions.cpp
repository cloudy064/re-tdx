#include "recon_contract_market_data_bonds_internal.hpp"

#include "recon_contract_internal.hpp"

#include <cmath>

namespace tdx::recon_contract_detail {

void validate_convertible_bond_subscriptions(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    const auto total = numeric_value(member_path(document, {"summary", "subscriptions"}));
    const auto listed = numeric_value(member_path(document, {"summary", "listed"}));
    const auto not_listed = numeric_value(member_path(document, {"summary", "not_listed"}));
    const auto formulas = numeric_value(member_path(document, {"summary", "formula_complete"}));
    const auto issue_size = numeric_value(member_path(document, {"summary", "issue_size_100m_yuan"}));
    const bool population =
        string_is(member(document, "schema"), "tdx-market-convertible-bonds-native-v1") &&
        string_is(member(document, "view"), "subscriptions") &&
        (string_is(member(document, "availability"), "live") ||
         string_is(member(document, "availability"), "stale-cache")) &&
        matched && total && listed && not_listed && formulas && issue_size &&
        *matched >= 300 && std::abs(*matched - *total) < 0.5 &&
        *listed >= 300 && *not_listed >= 0 &&
        std::abs(*listed + *not_listed - *matched) < 0.5 &&
        std::abs(*formulas - *matched) < 0.5 && *issue_size > 5000;
    add_assertion(result, "subscription_population", population,
                  "300+ rows with exact listed/not-listed reconciliation and complete formulas",
                  population);

    bool typed = true, formulas_reproduced = true, raw_units = true;
    const auto* rows = member(document, "subscriptions");
    if (rows && rows->is_array() && rows->size() >= 300) {
        for (const auto& row : rows->as_array()) {
            const auto* bond = member(row, "bond");
            const auto* underlying = member(row, "underlying");
            const auto* raw = member(row, "raw");
            typed = typed &&
                string_is(member(row, "kind"), "convertible-bond-subscription") &&
                bond && bond->is_object() && nonempty_string(member(*bond, "code")) &&
                underlying && underlying->is_object() &&
                nonempty_string(member(*underlying, "code")) &&
                nonempty_string(member(row, "subscription_date")) &&
                nonempty_string(member(row, "subscription_code")) &&
                nonempty_string(member(row, "conversion_start_date")) &&
                nonempty_string(member(row, "lottery_date")) &&
                nonempty_string(member(row, "event_id")) &&
                string_is(member(row, "source_resource"), "list/func_kkzss101_1.jsn") &&
                raw && raw->is_object();
            const auto stock_close = numeric_value(member(row, "underlying_close_yuan"));
            const auto conversion_price = numeric_value(member(row, "conversion_price_yuan"));
            const auto conversion_value = numeric_value(member(row, "conversion_value_yuan"));
            const auto bond_close = numeric_value(member(row, "bond_close_yuan"));
            const auto premium = numeric_value(member(row, "conversion_premium_pct"));
            if (!stock_close || !conversion_price || !conversion_value ||
                !bond_close || !premium || *conversion_price == 0.0 ||
                *conversion_value == 0.0) {
                formulas_reproduced = false;
            } else {
                const auto expected_value = *stock_close * 100.0 / *conversion_price;
                const auto expected_premium =
                    (*bond_close - expected_value) * 100.0 / expected_value;
                formulas_reproduced = formulas_reproduced &&
                    std::abs(*conversion_value - expected_value) < 0.000001 &&
                    std::abs(*premium - expected_premium) < 0.000001;
            }
            const auto* lottery_value = member(row, "lottery_rate_pct");
            const auto lottery = numeric_value(lottery_value);
            const auto issue = numeric_value(member(row, "issue_size_100m_yuan"));
            const auto* raw_lottery_value = raw ? member(*raw, "zql") : nullptr;
            const auto raw_lottery = numeric_value(raw_lottery_value);
            const auto raw_issue = raw ? numeric_value(member(*raw, "fxzs")) : std::nullopt;
            const bool lottery_units =
                (lottery && raw_lottery &&
                 std::abs(*lottery - *raw_lottery) < 0.000000001) ||
                (lottery_value && lottery_value->is_null() && raw_lottery_value &&
                 raw_lottery_value->is_string() &&
                 raw_lottery_value->as_string().empty());
            raw_units = raw_units && lottery_units && issue && raw_issue &&
                std::abs(*issue - *raw_issue) < 0.000001;
        }
    } else typed = false;
    add_assertion(result, "typed_subscription_rows", typed,
                  "bond/underlying identities, dates, source and raw evidence", typed);
    add_assertion(result, "client_formulas_reproduced", formulas_reproduced,
                  "zxj*100/zgj and (zxsp-zgjz)*100/zgjz", formulas_reproduced);
    add_assertion(result, "raw_units_preserved", raw_units,
                  "available lottery rates and all issue sizes equal raw CFG fields; pending lottery blanks remain null/empty pairs",
                  raw_units);
    const bool source_ok = source_exists(document, "list/func_kkzss101_1.jsn");
    add_assertion(result, "source", source_ok, "list/func_kkzss101_1.jsn", source_ok);

    const auto projection_count = numeric_value(member_path(
        document, {"summary", "new_bond_projection_rows"}));
    const auto projection_matches = numeric_value(member_path(
        document, {"new_bond_reconciliation", "exact_subscription_code_matches"}));
    const auto projection_conflicts = numeric_value(member_path(
        document, {"new_bond_reconciliation", "hybrid_or_stale_count"}));
    const auto* projection_rows = member(document, "new_bond_projection");
    bool projection_typed = projection_rows && projection_rows->is_array() &&
        projection_rows->size() >= 15;
    if (projection_typed) {
        for (const auto& row : projection_rows->as_array()) {
            const auto* match = member(row, "subscription_match");
            projection_typed = projection_typed &&
                string_is(member(row, "kind"), "new-convertible-bond-projection") &&
                nonempty_string(member_path(row, {"underlying", "code"})) &&
                nonempty_string(member(row, "subscription_code")) &&
                nonempty_string(member(row, "subscription_date")) &&
                string_is(member(row, "source_resource"),
                          "list/gxjty_zq_xkzz102_1.jsn") &&
                member(row, "raw") && member(row, "raw")->is_object() &&
                match && match->is_object() && bool_is(member(*match, "matched"), true) &&
                string_is(member(*match, "match_method"), "subscription-code");
        }
    }
    const bool projection_reconciled = projection_count && projection_matches &&
        projection_conflicts && *projection_count >= 15 &&
        std::abs(*projection_count - *projection_matches) < 0.5 &&
        *projection_conflicts >= 1 && projection_typed;
    add_assertion(result, "new_bond_projection_reconciled", projection_reconciled,
                  "15+ typed rows, all subscription codes matched, conflicts explicit",
                  projection_reconciled);
    const bool subscription_sources = source_exists(document, "list/func_kkzss101_1.jsn") &&
        source_exists(document, "list/gxjty_zq_xkzz102_1.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 2;
    add_assertion(result, "subscription_exact_sources", subscription_sources,
                  "two subscription/projection sources", subscription_sources);
}

}  // namespace tdx::recon_contract_detail
