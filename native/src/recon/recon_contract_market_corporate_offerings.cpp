#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {
namespace {

// private-placements-live
void validate_private_placements(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-futures-issuance-native-v1"),
                  "tdx-futures-issuance-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "section",
                  string_is(member(document, "section"), "placements"),
                  "placements", value_or_null(member(document, "section")));
    const auto* records = member_path(document, {"placement_summary", "records"});
    const auto* securities = member_path(document,
        {"placement_summary", "unique_securities"});
    const bool population = records && records->is_number() &&
        records->as_number() >= 1000 && securities && securities->is_number() &&
        securities->as_number() >= 800;
    add_assertion(result, "population", population,
                  "at least 1000 records and 800 securities", population);
    const auto* lifecycles = member_path(document,
        {"placement_summary", "lifecycle_counts"});
    std::set<std::string> lifecycle_labels;
    if (lifecycles && lifecycles->is_array())
        for (const auto& item : lifecycles->as_array()) {
            const auto* label = member(item, "label");
            if (label && label->is_string()) lifecycle_labels.insert(label->as_string());
        }
    const std::set<std::string> expected_lifecycles{
        "implemented", "implemented-locked", "implemented-unlocked",
        "plan-active", "plan-stopped", "registered"};
    add_assertion(result, "lifecycle_coverage",
                  lifecycle_labels == expected_lifecycles,
                  static_cast<std::uint64_t>(expected_lifecycles.size()),
                  static_cast<std::uint64_t>(lifecycle_labels.size()));
    const auto* rows = member(document, "private_placements");
    const Json* first = rows && rows->is_array() && !rows->as_array().empty()
        ? &rows->as_array().front() : nullptr;
    const Json* second = rows && rows->is_array() && rows->size() > 1
        ? &rows->as_array()[1] : nullptr;
    const auto* first_date = first ? member(*first, "sort_date") : nullptr;
    const auto* second_date = second ? member(*second, "sort_date") : nullptr;
    add_assertion(result, "descending_date_sort",
                  first_date && first_date->is_string() &&
                      first_date->as_string().size() == 8 &&
                      (!second_date || (second_date->is_string() &&
                       first_date->as_string() >= second_date->as_string())),
                  true, value_or_null(first_date));
    const bool row_semantics = first && member(*first, "security") &&
        member(*first, "security")->is_object() &&
        member(*first, "dates") && member(*first, "dates")->is_object() &&
        member(*first, "raw") && member(*first, "raw")->is_object() &&
        member(*first, "source_resource") &&
        member(*first, "source_resource")->is_string();
    add_assertion(result, "normalized_row", row_semantics,
                  "security, dates, raw, and source_resource", row_semantics);
    const auto* actual = member_path(document,
        {"placement_summary", "actual_gross_10k_yuan"});
    const auto* expected = member_path(document,
        {"placement_summary", "expected_raise_10k_yuan"});
    const bool amount_units = actual && actual->is_number() &&
        actual->as_number() > 1000000 && expected && expected->is_number() &&
        expected->as_number() > 1000000;
    add_assertion(result, "amount_units_10k_yuan", amount_units,
                  "both aggregates exceed 1,000,000 in 10k-yuan units", amount_units);
    const std::vector<std::string> required_sources{
        "list/func_qxfa201_1.jsn", "list/func_qxfa202_1.jsn",
        "list/func_qxfa301_1.jsn", "list/func_qxfa302_1.jsn",
        "list/func_qxfa402_1.jsn", "list/func_qxfa601_1.jsn"};
    bool all_sources = true;
    for (const auto& source : required_sources)
        all_sources = all_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", all_sources,
                  static_cast<std::uint64_t>(required_sources.size()), all_sources);
}

// rights-offerings-live
void validate_rights_offerings(const Json& document, Json& result) {
    const auto records = numeric_value(member_path(
        document, {"rights_summary", "records"}));
    const auto securities = numeric_value(member_path(
        document, {"rights_summary", "unique_securities"}));
    const auto implemented = numeric_value(member_path(
        document, {"rights_summary", "phase_counts", "implemented"}));
    const auto deliberating = numeric_value(member_path(
        document, {"rights_summary", "phase_counts", "deliberating"}));
    const auto abnormal = numeric_value(member_path(
        document, {"rights_summary", "phase_counts", "abnormal"}));
    const bool population =
        string_is(member(document, "schema"), "tdx-futures-issuance-native-v1") &&
        string_is(member(document, "section"), "rights") && records &&
        securities && implemented && deliberating && abnormal &&
        *records >= 100 && *securities >= 95 && *implemented >= 65 &&
        *deliberating >= 5 && *abnormal >= 25 &&
        std::abs(*implemented + *deliberating + *abnormal - *records) < 0.5;
    add_assertion(result, "rights_population", population,
                  "100+ rows across implemented/deliberating/abnormal phases",
                  population);
    const auto terminated = numeric_value(member_path(
        document, {"rights_summary", "status_counts", "已终止"}));
    const auto delayed = numeric_value(member_path(
        document, {"rights_summary", "status_counts", "已延期"}));
    const auto rejected = numeric_value(member_path(
        document, {"rights_summary", "status_counts", "未获准"}));
    const auto shareholder = numeric_value(member_path(
        document, {"rights_summary", "status_counts", "股东大会通过"}));
    const bool lifecycle = terminated && delayed && rejected && shareholder &&
        *terminated >= 20 && *delayed > 0 && *rejected > 0 && *shareholder > 0;
    add_assertion(result, "rights_lifecycle", lifecycle,
                  "implemented, shareholder-approved, terminated, delayed and rejected",
                  lifecycle);
    const auto actual = numeric_value(member_path(
        document, {"rights_summary", "implemented_raised_yuan"}));
    const auto planned = numeric_value(member_path(
        document, {"rights_summary", "planned_raised_yuan"}));
    const bool amounts = actual && planned && *actual > 200000000000.0 &&
        *planned > 100000000000.0;
    add_assertion(result, "yuan_aggregates", amounts,
                  "implemented/planned funds retain raw yuan units", amounts);

    bool typed = true, units = true;
    const auto* rows = member(document, "rights_offerings");
    if (rows && rows->is_array() && rows->size() >= 100) {
        for (const auto& row : rows->as_array()) {
            const auto* security = member(row, "security");
            const auto* raw = member(row, "raw");
            typed = typed && string_is(member(row, "kind"), "rights-offering") &&
                security && security->is_object() &&
                nonempty_string(member(*security, "code")) &&
                nonempty_string(member(row, "phase")) &&
                nonempty_string(member(row, "phase_label")) &&
                nonempty_string(member(row, "stage")) &&
                nonempty_string(member(row, "announcement_date")) &&
                nonempty_string(member(row, "sort_date")) &&
                nonempty_string(member(row, "amount_semantics")) &&
                nonempty_string(member(row, "event_id")) && raw && raw->is_object();
            const auto ratio = numeric_value(member(row, "rights_per_10_shares"));
            const auto shares = numeric_value(member(row, "offered_shares"));
            const auto raised = numeric_value(member(row, "raised_yuan"));
            const auto raw_ratio = raw ? numeric_value(member(*raw, "bl")) : std::nullopt;
            const auto raw_shares = raw ? numeric_value(member(*raw, "sl")) : std::nullopt;
            const auto raw_raised = raw ? numeric_value(member(*raw, "zj")) : std::nullopt;
            units = units && ratio && shares && raised && raw_ratio && raw_shares &&
                raw_raised && std::abs(*ratio - *raw_ratio) < 0.000001 &&
                std::abs(*shares - *raw_shares) < 0.01 &&
                std::abs(*raised - *raw_raised) < 0.01;
        }
    } else typed = false;
    add_assertion(result, "typed_rights_rows", typed,
                  "security, phase, dates, semantics, source ID and raw evidence",
                  typed);
    add_assertion(result, "raw_units_preserved", units,
                  "per-10 ratio, shares and yuan equal raw CFG fields", units);
    const bool sources_ok =
        source_exists(document, "list/func_qxfa107_1.jsn") &&
        source_exists(document, "list/func_qxfa108_1.jsn") &&
        source_exists(document, "list/func_qxfa109_1.jsn");
    add_assertion(result, "exact_sources", sources_ok, 3, sources_ok);
}

// preferred-shares-live
void validate_preferred_shares(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-futures-issuance-native-v1"),
                  "tdx-futures-issuance-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "section",
                  string_is(member(document, "section"), "preferred-shares"),
                  "preferred-shares", value_or_null(member(document, "section")));
    const auto records = numeric_value(member_path(
        document, {"preferred_share_summary", "records"}));
    const auto underlying = numeric_value(member_path(
        document, {"preferred_share_summary", "unique_underlying_securities"}));
    const auto codes = numeric_value(member_path(
        document, {"preferred_share_summary", "unique_preferred_codes"}));
    const auto issue_size = numeric_value(member_path(
        document, {"preferred_share_summary", "total_issue_size_yuan"}));
    const bool population = records && underlying && codes && issue_size &&
        *records >= 50 && *underlying >= 30 && *codes == *records &&
        *issue_size > 500000000000.0;
    add_assertion(result, "preferred_share_population", population,
                  "50+ issues, 30+ underlying stocks and 500bn+ yuan issuance",
                  population);
    bool typed = true, units = true;
    const auto* rows = member(document, "preferred_shares");
    if (rows && rows->is_array() && rows->size() >= 50) {
        for (const auto& row : rows->as_array()) {
            const auto* security = member(row, "underlying_security");
            const auto* raw = member(row, "raw");
            typed = typed && string_is(member(row, "kind"), "preferred-share") &&
                nonempty_string(member(row, "record_id")) && security &&
                security->is_object() && nonempty_string(member(*security, "code")) &&
                nonempty_string(member(row, "preferred_code")) &&
                nonempty_string(member(row, "listing_date")) &&
                nonempty_string(member(row, "source_resource")) &&
                raw && raw->is_object();
            const auto shares = numeric_value(member(row, "issue_shares"));
            const auto size = numeric_value(member(row, "issue_size_yuan"));
            const auto raw_shares = raw ? numeric_value(member(*raw, "fxsl")) : std::nullopt;
            const auto raw_size = raw ? numeric_value(member(*raw, "fxgm")) : std::nullopt;
            units = units && shares && size && raw_shares && raw_size &&
                std::abs(*shares - *raw_shares * 10000.0) < 0.01 &&
                std::abs(*size - *raw_size * 100000000.0) < 0.01;
        }
    } else typed = false;
    add_assertion(result, "typed_preferred_share_rows", typed,
                  "underlying security, preferred code, date, source and raw evidence",
                  typed);
    add_assertion(result, "cfg_units_converted", units,
                  "fxsl 万股 to shares and fxgm 亿元 to yuan", units);
    const bool exact_source =
        source_exists(document, "list/func_yxg101_3.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_yxg101_3.jsn", exact_source);
}

// employee-share-plans-live
void validate_employee_share_plans(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-employees-native-v1"),
                  "tdx-market-employees-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "share-plans"),
                  "share-plans", value_or_null(member(document, "view")));
    const auto* plans = member_path(document, {"share_plan_summary", "plans"});
    const auto* securities = member_path(
        document, {"share_plan_summary", "unique_securities"});
    const bool population = plans && plans->is_number() &&
        plans->as_number() >= 1000 && securities && securities->is_number() &&
        securities->as_number() >= 800;
    add_assertion(result, "population", population,
                  "at least 1000 plans and 800 securities", population);
    const auto* active = member_path(
        document, {"share_plan_summary", "active_plans"});
    const auto* completed = member_path(
        document, {"share_plan_summary", "completed_plans"});
    const bool statuses = active && active->is_number() &&
        active->as_number() > 0 && completed && completed->is_number() && plans &&
        plans->is_number() && active->as_number() + completed->as_number() ==
            plans->as_number();
    add_assertion(result, "status_reconciliation", statuses,
                  "active + completed = plans, with active plans", statuses);
    const auto* shares = member_path(
        document, {"share_plan_summary", "purchase_shares"});
    const auto* amount = member_path(
        document, {"share_plan_summary", "purchase_amount_yuan"});
    const bool amount_units = shares && shares->is_number() &&
        shares->as_number() > 1000000000.0 && amount && amount->is_number() &&
        amount->as_number() > 10000000000.0;
    add_assertion(result, "share_and_amount_units", amount_units,
                  "shares in single shares and amount in yuan", amount_units);
    const auto* rows = member(document, "share_plans");
    const Json* first = rows && rows->is_array() && !rows->as_array().empty()
        ? &rows->as_array().front() : nullptr;
    const Json* second = rows && rows->is_array() && rows->size() > 1
        ? &rows->as_array()[1] : nullptr;
    const auto* first_date = first ? member(*first, "sort_date") : nullptr;
    const auto* second_date = second ? member(*second, "sort_date") : nullptr;
    add_assertion(result, "descending_date_sort",
                  first_date && first_date->is_string() &&
                      first_date->as_string().size() == 8 &&
                      (!second_date || (second_date->is_string() &&
                       first_date->as_string() >= second_date->as_string())),
                  true, value_or_null(first_date));
    const auto* average_price = first ? member(*first, "purchase_average_price") : nullptr;
    const auto* row_amount = first ? member(*first, "purchase_amount_yuan") : nullptr;
    const bool amount_consistent = average_price && row_amount &&
        ((average_price->is_number() && row_amount->is_number()) ||
         (average_price->is_null() && row_amount->is_null()));
    const bool normalized = first && member(*first, "security") &&
        member(*first, "security")->is_object() &&
        member(*first, "dates") && member(*first, "dates")->is_object() &&
        member(*first, "active") && member(*first, "active")->is_bool() &&
        member(*first, "purchase_shares") &&
        member(*first, "purchase_shares")->is_number() &&
        amount_consistent &&
        member(*first, "raw") && member(*first, "raw")->is_object() &&
        string_is(member(*first, "source_resource"),
                  "list/func_qxfa501_1.jsn");
    add_assertion(result, "normalized_row", normalized,
                  "security, dates, status, shares, yuan amount, raw and source",
                  normalized);
    const bool exact_source = source_exists(
        document, "list/func_qxfa501_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_qxfa501_1.jsn", exact_source);
}

// hk-events-live
void validate_hk_events(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-hk-events-native-v1"),
                  "tdx-market-hk-events-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "all"),
                  "all", value_or_null(member(document, "view")));
    const auto* matched = member(document, "match_count");
    const auto* dividends = member_path(document, {"summary", "dividends"});
    const auto* holdings = member_path(
        document, {"summary", "holding_disclosures"});
    const auto* shorts = member_path(document, {"summary", "short_selling"});
    const auto* applications = member_path(
        document, {"summary", "listing_applications"});
    const bool family_shape = matched && matched->is_number() &&
        dividends && dividends->is_number() && holdings && holdings->is_number() &&
        shorts && shorts->is_number() && applications && applications->is_number();
    const bool reconciliation = family_shape &&
        dividends->as_number() + holdings->as_number() + shorts->as_number() +
            applications->as_number() == matched->as_number();
    const bool population = reconciliation && matched->as_number() >= 3000 &&
        dividends->as_number() >= 1000 && holdings->as_number() >= 500 &&
        shorts->as_number() >= 1000 && applications->as_number() >= 100;
    add_assertion(result, "population", population,
                  "substantial nonempty rolling populations across four HK event families",
                  population);
    add_assertion(result, "summary_reconciliation", reconciliation,
                  "four family counts equal match_count", reconciliation);
    const auto* rows = member(document, "rows");
    const Json* first = rows && rows->is_array() && !rows->as_array().empty()
        ? &rows->as_array().front() : nullptr;
    const Json* second = rows && rows->is_array() && rows->size() > 1
        ? &rows->as_array()[1] : nullptr;
    const auto* first_date = first ? member(*first, "date") : nullptr;
    const auto* second_date = second ? member(*second, "date") : nullptr;
    add_assertion(result, "descending_date_sort",
                  first_date && first_date->is_string() &&
                      first_date->as_string().size() == 8 &&
                      (!second_date || (second_date->is_string() &&
                       first_date->as_string() >= second_date->as_string())),
                  true, value_or_null(first_date));
    const Json* short_row = nullptr;
    if (rows && rows->is_array())
        for (const auto& row : rows->as_array())
            if (string_is(member(row, "kind"), "short-selling")) {
                short_row = &row;
                break;
            }
    const auto* shares_10k = short_row ? member(*short_row, "short_shares_10k") : nullptr;
    const auto* shares = short_row ? member(*short_row, "short_shares") : nullptr;
    const auto* amount_10k = short_row
        ? member(*short_row, "short_amount_10k_currency_units") : nullptr;
    const auto* amount = short_row
        ? member(*short_row, "short_amount_currency_units") : nullptr;
    const auto* turnover = short_row
        ? member(*short_row, "turnover_currency_units") : nullptr;
    const auto* ratio = short_row ? member(*short_row, "short_turnover_pct") : nullptr;
    const bool units = shares_10k && shares_10k->is_number() &&
        shares && shares->is_number() && amount_10k && amount_10k->is_number() &&
        amount && amount->is_number() && turnover && turnover->is_number() &&
        ratio && ratio->is_number() &&
        std::abs(shares->as_number() - shares_10k->as_number() * 10000.0) < 0.01 &&
        std::abs(amount->as_number() - amount_10k->as_number() * 10000.0) < 0.01 &&
        std::abs(ratio->as_number() - amount->as_number() * 100.0 /
                 turnover->as_number()) < 0.000001;
    add_assertion(result, "short_units", units,
                  "10k units converted to shares/currency and ratio reconciled", units);
    const std::vector<std::string> required_sources{
        "list/func_ggrl102_1.jsn", "list/func_ggrl103_1.jsn",
        "list/func_ggrl104_1.jsn", "list/func_ggrl105_1.jsn"};
    bool all_sources = true;
    for (const auto& source : required_sources)
        all_sources = all_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", all_sources,
                  static_cast<std::uint64_t>(required_sources.size()), all_sources);
}

// hk-short-history-live
void validate_hk_short_history(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-hk-short-history-native-v1"),
                  "tdx-market-hk-short-history-native-v1",
                  value_or_null(member(document, "schema")));
    const bool identity =
        string_is(member_path(document, {"security", "market"}), "hk") &&
        string_is(member_path(document, {"security", "code"}), "00700") &&
        number_is(member_path(document, {"security", "market_id"}), 31);
    add_assertion(result, "security_identity", identity,
                  "31:00700 resolves as HK00700", identity);
    const auto* count = member_path(document, {"summary", "history_count"});
    const auto* history = member(document, "history");
    const bool population = count && count->is_number() && count->as_number() >= 30 &&
        history && history->is_array() && history->size() == count->as_number();
    add_assertion(result, "history_population", population,
                  "at least 30 chronological daily rows", value_or_null(count));
    const bool source =
        string_is(member_path(document, {"source", "auxiliary_field"}),
                  "hk_short_volume") &&
        string_is(member_path(document, {"source", "transport"}),
                  "tdx-7727-0x23ff") &&
        number_is(member_path(document, {"source", "volume_lot_size_shares"}), 100);
    add_assertion(result, "wire_semantics", source,
                  "7727 auxiliary field and 100-share HK lot", source);
    const auto* overlap = member_path(document, {"reconciliation", "overlap_day_count"});
    const auto* exact = member_path(document, {"reconciliation", "exact_match_count"});
    const auto* mismatch = member_path(document, {"reconciliation", "mismatch_count"});
    const bool reconciled = overlap && overlap->is_number() && overlap->as_number() >= 1 &&
        exact && exact->is_number() && exact->as_number() == overlap->as_number() &&
        mismatch && mismatch->is_number() && mismatch->as_number() == 0 &&
        bool_is(member_path(document, {"reconciliation", "all_overlaps_exact"}), true);
    add_assertion(result, "event_reconciliation", reconciled,
                  "all overlapping GGRL104 share counts match exactly", reconciled);
    bool normalized_rows = population;
    bool found_event_match = false;
    if (history && history->is_array()) {
        for (const auto& row : history->as_array()) {
            const auto* short_shares = member(row, "short_shares");
            const auto* lots = member(row, "volume_lots");
            const auto* shares = member(row, "volume_shares");
            const auto* ratio = member(row, "short_share_volume_pct");
            const bool zero_volume_ratio = short_shares && short_shares->is_number() &&
                number_is(shares, 0.0) && number_is(short_shares, 0.0) &&
                ratio && ratio->is_null();
            const bool positive_volume_ratio = shares && shares->is_number() &&
                shares->as_number() > 0.0 && ratio && ratio->is_number();
            normalized_rows = normalized_rows && short_shares && short_shares->is_number() &&
                short_shares->as_number() >= 0 && lots && lots->is_number() &&
                shares && shares->is_number() &&
                (zero_volume_ratio || positive_volume_ratio) &&
                std::abs(shares->as_number() - lots->as_number() * 100.0) < 0.01;
            if (bool_is(member(row, "event_exact_match"), true)) found_event_match = true;
        }
    }
    add_assertion(result, "normalized_history", normalized_rows && found_event_match,
                  "share units, lot conversion, positive-volume ratios or null 0/0 ratio, and an exact event row",
                  normalized_rows && found_event_match);
    const bool event_source = source_exists(document, "list/func_ggrl104_1.jsn");
    add_assertion(result, "event_source", event_source,
                  "list/func_ggrl104_1.jsn", event_source);
}

struct OfferingContract {
    std::string_view id;
    MarketCorporateCatalogContractValidator validate;
};

constexpr std::array<OfferingContract, 6> offering_contracts{{
    {"private-placements-live", validate_private_placements},
    {"rights-offerings-live", validate_rights_offerings},
    {"preferred-shares-live", validate_preferred_shares},
    {"employee-share-plans-live", validate_employee_share_plans},
    {"hk-events-live", validate_hk_events},
    {"hk-short-history-live", validate_hk_short_history},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < offering_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < offering_contracts.size(); ++right)
            if (offering_contracts[left].id == offering_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_corporate_offerings_contract(
    const std::string& contract_id, const Json& document,
    const Json&, Json& result) {
    for (const auto& contract : offering_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
