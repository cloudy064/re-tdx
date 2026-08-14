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

// ipo-guidance-live
void validate_ipo_guidance(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    const auto guidance = numeric_value(member_path(
        document, {"summary", "ipo_guidance"}));
    bool typed = string_is(member(document, "schema"),
                           "tdx-market-calendar-native-v1") &&
        string_is(member(document, "view"), "ipo-guidance") &&
        string_is(member(document, "availability"), "live") &&
        matched && guidance && *matched >= 1200 &&
        std::abs(*matched - *guidance) < 0.5;

    const auto* boards = member_path(
        document, {"summary", "ipo_guidance_boards"});
    const auto* progresses = member_path(
        document, {"summary", "ipo_guidance_progress"});
    const auto* regions = member_path(
        document, {"summary", "ipo_guidance_regions"});
    double board_total = 0, progress_total = 0, region_total = 0;
    bool four_boards = boards && boards->is_object();
    for (const auto* board : {"主板", "创业板", "科创板", "北交所"}) {
        const auto count = boards ? numeric_value(member(*boards, board))
                                  : std::nullopt;
        four_boards = four_boards && count && *count > 0;
    }
    if (boards && boards->is_object())
        for (const auto& [_, value] : boards->as_object())
            if (value.is_number()) board_total += value.as_number();

    bool lifecycle = progresses && progresses->is_object();
    for (const auto* progress : {"辅导备案", "撤回辅导备案", "辅导验收",
                                 "辅导工作完成", "辅导中", "终止辅导"}) {
        const auto count = progresses
            ? numeric_value(member(*progresses, progress)) : std::nullopt;
        lifecycle = lifecycle && count && *count > 0;
    }
    if (progresses && progresses->is_object())
        for (const auto& [_, value] : progresses->as_object())
            if (value.is_number()) progress_total += value.as_number();

    bool region_coverage = regions && regions->is_object();
    for (const auto* region : {"江苏", "浙江", "广东", "北京"}) {
        const auto count = regions ? numeric_value(member(*regions, region))
                                   : std::nullopt;
        region_coverage = region_coverage && count && *count > 0;
    }
    if (regions && regions->is_object())
        for (const auto& [_, value] : regions->as_object())
            if (value.is_number()) region_total += value.as_number();
    const bool summaries_reconcile = matched &&
        std::abs(board_total - *matched) < 0.5 &&
        std::abs(progress_total - *matched) < 0.5 &&
        std::abs(region_total - *matched) < 0.5;
    add_assertion(result, "typed_ipo_guidance_population", typed,
                  "1200+ IPO listing-guidance rows", typed);
    add_assertion(result, "guidance_dimensions",
                  four_boards && lifecycle && region_coverage &&
                      summaries_reconcile,
                  "four boards, six key progress states and major regions reconcile",
                  four_boards && lifecycle && region_coverage &&
                      summaries_reconcile);

    bool auditable = true;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) {
        typed = typed && rows->size() >= 1200;
        for (const auto& row : rows->as_array()) {
            const auto* raw = member(row, "raw");
            const auto* record_id = member(row, "source_record_id");
            const auto* raw_id = raw && raw->is_object()
                ? member(*raw, "$ZQDM") : nullptr;
            typed = typed &&
                string_is(member(row, "kind"), "ipo-guidance") &&
                nonempty_string(member(row, "date")) &&
                nonempty_string(member(row, "company_name")) &&
                nonempty_string(member(row, "guidance_progress")) &&
                nonempty_string(member(row, "board_category")) &&
                nonempty_string(member(row, "guidance_institution")) &&
                nonempty_string(member(row, "region")) &&
                nonempty_string(member(row, "content")) &&
                nonempty_string(member(row, "event_id")) &&
                nonempty_string(record_id) && nonempty_string(raw_id) &&
                record_id->as_string() == raw_id->as_string() &&
                string_is(member(row, "source_resource"),
                          "list/func_zdgz_qzkcbd101_1.jsn");
            auditable = auditable && raw && raw->is_object();
        }
    } else typed = false;
    add_assertion(result, "typed_guidance_rows", typed,
                  "typed progress/institution/region rows retain source IDs",
                  typed);
    add_assertion(result, "raw_audit", auditable,
                  "all listing-guidance rows retain raw evidence", auditable);
    const bool source_ok = source_exists(
        document, "list/func_zdgz_qzkcbd101_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_zdgz_qzkcbd101_1.jsn", source_ok);
}

// ipo-review-live
void validate_ipo_review(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    const auto reviews = numeric_value(member_path(
        document, {"summary", "ipo_reviews"}));
    bool typed = string_is(member(document, "schema"),
                           "tdx-market-calendar-native-v1") &&
        string_is(member(document, "view"), "ipo-review") &&
        string_is(member(document, "availability"), "live") &&
        matched && reviews && *matched >= 500 &&
        std::abs(*matched - *reviews) < 0.5;
    const auto* boards = member_path(document, {"summary", "ipo_review_boards"});
    const auto* statuses = member_path(document, {"summary", "ipo_review_statuses"});
    double board_total = 0, status_total = 0;
    bool four_boards = boards && boards->is_object();
    for (const auto* board : {"主板", "创业板", "科创板", "北交所"}) {
        const auto count = boards ? numeric_value(member(*boards, board)) : std::nullopt;
        four_boards = four_boards && count && *count > 0;
    }
    if (boards && boards->is_object())
        for (const auto& [_, value] : boards->as_object())
            if (value.is_number()) board_total += value.as_number();
    bool lifecycle = statuses && statuses->is_object();
    for (const auto* status : {"已受理", "已问询", "上会通过", "提交注册",
                               "注册生效", "中止", "终止"}) {
        const auto count = statuses
            ? numeric_value(member(*statuses, status)) : std::nullopt;
        lifecycle = lifecycle && count && *count > 0;
    }
    if (statuses && statuses->is_object())
        for (const auto& [_, value] : statuses->as_object())
            if (value.is_number()) status_total += value.as_number();
    const bool summaries_reconcile = matched &&
        std::abs(board_total - *matched) < 0.5 &&
        std::abs(status_total - *matched) < 0.5;
    add_assertion(result, "typed_ipo_review_population", typed,
                  "500+ IPO review rows", typed);
    add_assertion(result, "board_and_lifecycle_coverage",
                  four_boards && lifecycle && summaries_reconcile,
                  "four boards and seven review lifecycle states reconcile",
                  four_boards && lifecycle && summaries_reconcile);

    std::size_t financing_rows = 0, ratio_rows = 0, prospectus_rows = 0;
    bool auditable = true, units_preserved = true, ratio_reconciled = true;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) {
        typed = typed && rows->size() >= 500;
        for (const auto& row : rows->as_array()) {
            const auto* raw = member(row, "raw");
            typed = typed && string_is(member(row, "kind"), "ipo-review") &&
                member(row, "company_name") && member(row, "review_status") &&
                member(row, "board_category") && member(row, "sponsor") &&
                string_is(member(row, "source_resource"),
                          "list/func_zdgz_kcbsq101_1.jsn");
            auditable = auditable && raw && raw->is_object();
            const auto* prospectus = member(row, "prospectus_url");
            if (prospectus && prospectus->is_string() &&
                prospectus->as_string().rfind("http", 0) == 0) ++prospectus_rows;
            const auto financing = numeric_value(
                member(row, "planned_financing_yuan"));
            const auto raw_financing = raw && raw->is_object()
                ? numeric_value(member(*raw, "rzje")) : std::nullopt;
            if (financing) {
                ++financing_rows;
                units_preserved = units_preserved && raw_financing &&
                    std::abs(*financing - *raw_financing) < 0.01;
            }
            const auto pre = numeric_value(member(row, "pre_issue_total_shares"));
            const auto issue = numeric_value(member(row, "planned_issue_shares"));
            const auto ratio = numeric_value(member(row, "post_issue_share_pct"));
            if (pre && issue && ratio && *pre + *issue > 0) {
                ++ratio_rows;
                ratio_reconciled = ratio_reconciled &&
                    std::abs(*ratio - 100.0 * *issue / (*pre + *issue)) < 0.02;
            }
        }
    } else typed = false;
    add_assertion(result, "typed_review_rows", typed,
                  "typed status/sponsor/board rows retain raw evidence", typed);
    const bool field_coverage = financing_rows >= 500 && ratio_rows >= 490 &&
                                prospectus_rows >= 500;
    add_assertion(result, "review_field_coverage", field_coverage,
                  "500+ financing/prospectus and 490+ share-ratio rows", field_coverage);
    add_assertion(result, "review_units_and_share_ratio",
                  auditable && units_preserved && ratio_reconciled,
                  "yuan fields equal raw; issue/(pre+issue) reproduces percent",
                  auditable && units_preserved && ratio_reconciled);
    const bool source_ok = source_exists(
        document, "list/func_zdgz_kcbsq101_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_zdgz_kcbsq101_1.jsn", source_ok);
}

// ipo-subscriptions-live
void validate_ipo_subscriptions(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    bool typed = string_is(member(document, "schema"),
                           "tdx-market-calendar-native-v1") &&
        string_is(member(document, "view"), "ipo-subscriptions") &&
        string_is(member(document, "availability"), "live") &&
        matched && *matched >= 30;
    std::set<std::string> boards;
    std::size_t priced = 0, financing_rows = 0, ratio_rows = 0;
    bool auditable = true, units_preserved = true, ratio_reconciled = true;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) {
        typed = typed && rows->size() >= 30;
        for (const auto& row : rows->as_array()) {
            const auto* security = member(row, "security");
            const auto* raw = member(row, "raw");
            typed = typed && string_is(member(row, "kind"), "ipo-subscription") &&
                security && security->is_object() &&
                member(*security, "market") && member(*security, "code") &&
                string_is(member(row, "source_resource"),
                          "list/func_zdgz_kcbsq103_1.jsn");
            auditable = auditable && raw && raw->is_object();
            const auto* board = member(row, "board_category");
            if (board && board->is_string() && !board->as_string().empty())
                boards.insert(board->as_string());
            if (numeric_value(member(row, "issue_price"))) ++priced;
            const auto financing = numeric_value(
                member(row, "planned_financing_yuan"));
            const auto raw_financing = raw && raw->is_object()
                ? numeric_value(member(*raw, "rzje")) : std::nullopt;
            if (financing) {
                ++financing_rows;
                units_preserved = units_preserved && raw_financing &&
                    std::abs(*financing - *raw_financing) < 0.01;
            }
            const auto pre = numeric_value(member(row, "pre_issue_total_shares"));
            const auto issue = numeric_value(member(row, "planned_issue_shares"));
            const auto ratio = numeric_value(member(row, "post_issue_share_pct"));
            if (pre && issue && ratio && *pre + *issue > 0) {
                ++ratio_rows;
                ratio_reconciled = ratio_reconciled &&
                    std::abs(*ratio - 100.0 * *issue / (*pre + *issue)) < 0.02;
            }
        }
    } else typed = false;
    add_assertion(result, "typed_ipo_subscription_rows", typed,
                  "30+ typed IPO subscription rows", typed);
    const bool four_boards = boards.count("主板") && boards.count("创业板") &&
        boards.count("科创板") && boards.count("北交所");
    add_assertion(result, "four_board_coverage", four_boards,
                  "主板,创业板,科创板,北交所", four_boards);
    const bool field_coverage = priced >= 30 && financing_rows >= 30 &&
                                ratio_rows >= 30;
    add_assertion(result, "issuance_field_coverage", field_coverage,
                  "30+ priced, financing and share-structure rows", field_coverage);
    add_assertion(result, "source_units_and_share_ratio",
                  auditable && units_preserved && ratio_reconciled,
                  "yuan fields equal raw; issue/(pre+issue) reproduces percent",
                  auditable && units_preserved && ratio_reconciled);
    const bool source_ok = source_exists(
        document, "list/func_zdgz_kcbsq103_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_zdgz_kcbsq103_1.jsn", source_ok);
}

// ipo-subscription-details-live
void validate_ipo_subscription_details(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    bool typed = string_is(member(document, "schema"),
                           "tdx-market-calendar-native-v1") &&
        string_is(member(document, "view"), "ipo-subscription-details") &&
        string_is(member(document, "availability"), "live") &&
        matched && *matched >= 40;
    std::size_t reconciled_units = 0, listed = 0;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) {
        typed = typed && rows->size() >= 40;
        for (const auto& row : rows->as_array()) {
            const auto* security = member(row, "security");
            const auto* raw = member(row, "raw");
            typed = typed && string_is(member(row, "kind"),
                    "ipo-subscription-detail") &&
                security && security->is_object() &&
                string_is(member(*security, "market"), "bj") &&
                member(row, "subscription_code") &&
                member(row, "subscription_date") &&
                member(row, "issue_total_shares") &&
                member(row, "online_issue_shares") &&
                raw && raw->is_object();
            const auto total = numeric_value(member(row, "issue_total_shares"));
            const auto raised = numeric_value(member(row, "raised_yuan"));
            const auto raw_total = raw && raw->is_object()
                ? numeric_value(member(*raw, "fxzl")) : std::nullopt;
            const auto raw_raised = raw && raw->is_object()
                ? numeric_value(member(*raw, "fxmz")) : std::nullopt;
            const auto optional_pair = [](const Json* normalized_value,
                                          const Json* raw_value,
                                          double tolerance) {
                const auto normalized = numeric_value(normalized_value);
                const auto source = numeric_value(raw_value);
                if (normalized && source)
                    return std::abs(*normalized - *source) < tolerance;
                return normalized_value && normalized_value->is_null() && raw_value &&
                    raw_value->is_string() && raw_value->as_string().empty();
            };
            if (total && raised && raw_total && raw_raised &&
                std::abs(*total - *raw_total) < 0.01 &&
                std::abs(*raised - *raw_raised) < 0.01 &&
                optional_pair(member(row, "online_issue_shares"),
                              raw && raw->is_object() ? member(*raw, "fxws") : nullptr,
                              0.01) &&
                optional_pair(member(row, "winning_rate_pct"),
                              raw && raw->is_object() ? member(*raw, "zql") : nullptr,
                              0.000001)) ++reconciled_units;
            const auto* listing = member(row, "listing_date");
            typed = typed && listing && listing->is_string();
            if (listing && listing->is_string() && !listing->as_string().empty()) ++listed;
        }
    } else typed = false;
    add_assertion(result, "typed_bse_ipo_details", typed,
                  "40+ BSE subscription detail rows retain dates and share structure", typed);
    const bool units = rows && rows->is_array() && rows->size() >= 40 &&
        reconciled_units == rows->size() && listed > 0 && listed <= rows->size();
    add_assertion(result, "source_units", units,
                  "all rows preserve mandatory units; pending online/rate fields remain null/empty pairs",
                  units);
    const bool source_ok = source_exists(document, "list/func_sbxg101_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_sbxg101_1.jsn", source_ok);
}

// us-ipo-live
void validate_us_ipo(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    const auto applications = numeric_value(member_path(
        document, {"summary", "us_ipo_applications"}));
    const auto calendar = numeric_value(member_path(
        document, {"summary", "us_ipo_calendar"}));
    const auto listed_count = numeric_value(member_path(
        document, {"summary", "us_ipo_listed"}));
    const auto pending = numeric_value(member_path(
        document, {"summary", "us_ipo_pending"}));
    const auto overlap = numeric_value(member_path(
        document, {"summary", "us_ipo_calendar_listed_code_overlap"}));
    const auto same_date = numeric_value(member_path(
        document, {"summary", "us_ipo_calendar_listed_same_date"}));
    const auto changed_date = numeric_value(member_path(
        document, {"summary", "us_ipo_calendar_listed_date_changed"}));
    const bool population = matched && applications && calendar && listed_count && pending &&
        *applications >= 700 && *calendar >= 2200 && *listed_count >= 390 &&
        *pending >= 1 &&
        std::abs(*matched - (*applications + *calendar + *listed_count + *pending)) < 0.5;
    add_assertion(result, "four_phase_population", population,
                  "700+ applications, 2200+ schedules, 390+ listings and pending rows reconcile",
                  population);
    const bool lifecycle = overlap && same_date && changed_date &&
        *overlap >= 390 && *changed_date >= 1 &&
        std::abs(*overlap - *same_date - *changed_date) < 0.5;
    add_assertion(result, "schedule_listing_reconciliation", lifecycle,
                  "scheduled/listed code overlap splits into same-date and changed-date rows",
                  lifecycle);
    bool typed = string_is(member(document, "schema"),
                           "tdx-market-calendar-native-v1") &&
        string_is(member(document, "view"), "us-ipo") &&
        string_is(member(document, "availability"), "live");
    std::set<std::string> phases;
    std::size_t converted_amounts = 0, converted_shares = 0;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) for (const auto& row : rows->as_array()) {
        const auto* security = member(row, "security");
        const auto* raw = member(row, "raw");
        const auto* kind = member(row, "kind");
        typed = typed && security && security->is_object() &&
            string_is(member(*security, "market"), "us") && kind && kind->is_string() &&
            raw && raw->is_object() && member(row, "source_resource");
        if (!kind || !kind->is_string() || !raw || !raw->is_object()) continue;
        phases.insert(kind->as_string());
        const bool million = kind->as_string() == "us-ipo-application" ||
            kind->as_string() == "us-ipo-calendar";
        const auto amount = numeric_value(member(row, "issue_amount_usd"));
        const auto shares = numeric_value(member(row, "issue_shares"));
        const auto raw_amount = numeric_value(member(*raw, "fxze"));
        const auto raw_shares = numeric_value(member(*raw, "fxgfs"));
        const double scale = million ? 1000000.0 : 1.0;
        if (amount && raw_amount &&
            std::abs(*amount - *raw_amount * scale) < 0.01) ++converted_amounts;
        if (shares && raw_shares &&
            std::abs(*shares - *raw_shares * scale) < 0.01) ++converted_shares;
    } else typed = false;
    const bool all_phases = phases.count("us-ipo-application") &&
        phases.count("us-ipo-calendar") && phases.count("us-ipo-listed") &&
        phases.count("us-ipo-pending");
    add_assertion(result, "typed_us_ipo_rows", typed && all_phases,
                  "all four phases retain US identity, source and raw evidence",
                  typed && all_phases);
    const bool conversions = converted_amounts >= 3200 && converted_shares >= 2600;
    add_assertion(result, "source_unit_conversion", conversions,
                  "3200+ amounts and 2600+ share counts convert MGRL million units while preserving MGXG base units",
                  conversions);
    const bool sources_ok = source_exists(document, "list/func_mgrl101_1.jsn") &&
        source_exists(document, "list/func_mgrl102_1.jsn") &&
        source_exists(document, "list/func_mgxg101_1.jsn") &&
        source_exists(document, "list/func_mgxg102_1.jsn");
    add_assertion(result, "phase_sources", sources_ok, 4, sources_ok);
}

struct IpoContract {
    std::string_view id;
    MarketCorporateCatalogContractValidator validate;
};

constexpr std::array<IpoContract, 5> ipo_contracts{{
    {"ipo-guidance-live", validate_ipo_guidance},
    {"ipo-review-live", validate_ipo_review},
    {"ipo-subscriptions-live", validate_ipo_subscriptions},
    {"ipo-subscription-details-live", validate_ipo_subscription_details},
    {"us-ipo-live", validate_us_ipo},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < ipo_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < ipo_contracts.size(); ++right)
            if (ipo_contracts[left].id == ipo_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_corporate_ipo_contract(
    const std::string& contract_id, const Json& document,
    const Json&, Json& result) {
    for (const auto& contract : ipo_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
