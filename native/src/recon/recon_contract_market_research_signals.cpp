#include "recon_contract_market_research_internal.hpp"

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

using SignalContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_block_rotation(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-block-rotation-native-v1"),
                  "tdx-market-block-rotation-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "category",
                  string_is(member_path(document, {"filters", "category"}), "all"),
                  "all", value_or_null(member_path(document, {"filters", "category"})));
    add_assertion(result, "period",
                  string_is(member_path(document, {"filters", "period"}), "1w"),
                  "1w", value_or_null(member_path(document, {"filters", "period"})));
    const auto* source_rows = member_path(document, {"counts", "source_rows"});
    add_assertion(result, "four_category_coverage",
                  source_rows && source_rows->is_number() &&
                      source_rows->as_number() >= 500,
                  "number >= 500", value_or_null(source_rows));
    const auto* categories = member_path(document, {"summary", "by_category"});
    add_assertion(result, "category_summaries",
                  categories && categories->is_array() && categories->size() == 4,
                  "four categories",
                  categories ? Json(static_cast<std::uint64_t>(categories->size()))
                             : Json(nullptr));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    add_assertion(result, "records_present", first && second, "at least two rows",
                  records ? Json(static_cast<std::uint64_t>(records->size())) : Json(nullptr));
    const auto* block = first ? member(*first, "block") : nullptr;
    const auto* code = block ? member(*block, "code") : nullptr;
    const auto* name_resolved = block ? member(*block, "name_resolved") : nullptr;
    add_assertion(result, "block_identity",
                  block && block->is_object() && code && code->is_string() &&
                      code->as_string().size() == 6 &&
                      name_resolved && name_resolved->is_bool() &&
                      name_resolved->as_bool(),
                  "resolved six-digit block", value_or_null(block));
    const auto* first_count = first
        ? member_path(*first, {"periods", "1w", "anomaly_count"}) : nullptr;
    const auto* second_count = second
        ? member_path(*second, {"periods", "1w", "anomaly_count"}) : nullptr;
    add_assertion(result, "descending_anomaly_sort",
                  first_count && second_count && first_count->is_number() &&
                      second_count->is_number() &&
                      first_count->as_number() >= second_count->as_number(),
                  true,
                  first_count && second_count && first_count->is_number() &&
                          second_count->is_number()
                      ? Json(first_count->as_number() >= second_count->as_number())
                      : Json(nullptr));
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    bool four_sources = true;
    for (const auto* resource : {"list/func_bkld101_1.jsn",
                                 "list/func_bkld102_1.jsn",
                                 "list/func_bkld103_1.jsn",
                                 "list/func_bkld104_1.jsn"})
        four_sources = four_sources && source_exists(document, resource);
    add_assertion(result, "exact_sources", four_sources, true, four_sources);
    const auto* sources = member(document, "sources");
    bool healthy_sources = sources && sources->is_array() && sources->size() == 4;
    if (healthy_sources) {
        for (const auto& source : sources->as_array()) {
            const auto* attempts = member(source, "attempts");
            const auto* stale = member(source, "stale");
            healthy_sources = healthy_sources && attempts && attempts->is_number() &&
                attempts->as_number() >= 1 && stale && stale->is_bool();
        }
    }
    add_assertion(result, "source_health", healthy_sources, true, healthy_sources);
}

void validate_limit_ladder(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-limit-ladder-native-v1"),
                  "tdx-market-limit-ladder-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "category",
                  string_is(member_path(document, {"filters", "category"}), "all"),
                  "all", value_or_null(member_path(document, {"filters", "category"})));
    add_assertion(result, "sort",
                  string_is(member_path(document, {"filters", "sort"}), "total-height"),
                  "total-height", value_or_null(member_path(document, {"filters", "sort"})));
    const auto* source_rows = member_path(document, {"counts", "source_rows"});
    const auto* category_rows = member_path(document, {"counts", "category_rows"});
    add_assertion(result, "block_coverage",
                  source_rows && source_rows->is_number() &&
                      source_rows->as_number() >= 2 && category_rows &&
                      category_rows->is_number() &&
                      category_rows->as_number() == source_rows->as_number(),
                  "daily active source rows reconcile to categorized rows; market activity count is not fixed",
                  value_or_null(source_rows));
    const auto* categories = member_path(document, {"summary", "by_category"});
    add_assertion(result, "category_summaries",
                  categories && categories->is_array() && categories->size() == 2,
                  "industry and concept",
                  categories ? Json(static_cast<std::uint64_t>(categories->size()))
                             : Json(nullptr));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    add_assertion(result, "records_present", first && second, "at least two rows",
                  records ? Json(static_cast<std::uint64_t>(records->size())) : Json(nullptr));
    const auto* block = first ? member(*first, "block") : nullptr;
    const auto* code = block ? member(*block, "code") : nullptr;
    const auto* name_resolved = block ? member(*block, "name_resolved") : nullptr;
    const auto* members_available = block ? member(*block, "members_available") : nullptr;
    add_assertion(result, "block_identity_and_members",
                  block && block->is_object() && code && code->is_string() &&
                      code->as_string().size() == 6 && name_resolved &&
                      name_resolved->is_bool() && name_resolved->as_bool() &&
                      members_available && members_available->is_bool() &&
                      members_available->as_bool(),
                  "resolved block with local members", value_or_null(block));
    const auto* first_height = first ? member(*first, "sum_streak_heights") : nullptr;
    const auto* second_height = second ? member(*second, "sum_streak_heights") : nullptr;
    add_assertion(result, "descending_total_height_sort",
                  first_height && second_height && first_height->is_number() &&
                      second_height->is_number() &&
                      first_height->as_number() >= second_height->as_number(),
                  true,
                  first_height && second_height && first_height->is_number() &&
                          second_height->is_number()
                      ? Json(first_height->as_number() >= second_height->as_number())
                      : Json(nullptr));
    const auto* checked = member_path(
        document, {"summary", "advancement_formula_checked_blocks"});
    const auto* mismatches = member_path(
        document, {"summary", "advancement_formula_mismatch_blocks"});
    add_assertion(result, "advancement_formula",
                  checked && checked->is_number() && checked->as_number() >= 1 &&
                      mismatches && mismatches->is_number() &&
                      mismatches->as_number() == 0,
                  "at least one checked row and zero mismatches",
                  value_or_null(mismatches));
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const bool exact_source = source_exists(document, "list/func_lbtt101_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_lbtt101_1.jsn", exact_source);
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && !sources->as_array().empty()
        ? &sources->as_array().front() : nullptr;
    const auto* attempts = source ? member(*source, "attempts") : nullptr;
    const auto* stale = source ? member(*source, "stale") : nullptr;
    add_assertion(result, "source_health",
                  attempts && attempts->is_number() && attempts->as_number() >= 1 &&
                      stale && stale->is_bool(), true,
                  attempts && stale ? Json(true) : Json(false));
}

void validate_threshold_stocks_high_price(const std::string& contract_id, const Json& document,
        Json& result) {
    const std::string universe = contract_id == "threshold-stocks-high-price-live"
        ? "high-price" : "mega-cap";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-threshold-stocks-native-v1"),
                  "tdx-market-threshold-stocks-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "view", string_is(member(document, "view"), "members"),
                  "members", value_or_null(member(document, "view")));
    add_assertion(result, "universe", string_is(member(document, "universe"), universe),
                  universe, value_or_null(member(document, "universe")));
    const auto* selected = member(document, "selected_period");
    const auto* date = selected ? member(*selected, "date") : nullptr;
    const auto* total = selected ? member(*selected, "total_count") : nullptr;
    add_assertion(result, "selected_period",
                  selected && selected->is_object() && date && date->is_string() &&
                      date->as_string().size() == 8 && total && total->is_number() &&
                      total->as_number() >= 100,
                  "dated period with at least 100 active members",
                  value_or_null(selected));
    const auto* members = member_path(document, {"summary", "members"});
    const bool reconciled = members && members->is_object() &&
        bool_is(member(*members, "active_count_matches"), true) &&
        bool_is(member(*members, "entered_count_matches"), true) &&
        bool_is(member(*members, "exited_count_matches"), true);
    add_assertion(result, "member_reconciliation", reconciled, true, reconciled);
    const auto* trend_matches = member_path(
        document, {"summary", "trend_check", "selected_count_matches"});
    add_assertion(result, "trend_reconciliation",
                  bool_is(trend_matches, true), true, value_or_null(trend_matches));
    const auto* trend = member(document, "trend");
    add_assertion(result, "trend_history",
                  trend && trend->is_array() && trend->size() >= 100,
                  "at least 100 points",
                  trend ? Json(static_cast<std::uint64_t>(trend->size())) : Json(nullptr));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "member_identity",
                  security && security->is_object() &&
                      bool_is(member(*security, "name_resolved"), true) &&
                      raw && raw->is_object(),
                  "resolved security with raw row", value_or_null(security));
    const char* metric = universe == "high-price"
        ? "close_price_yuan" : "market_cap_100m_yuan";
    const auto* first_metric = first ? member(*first, metric) : nullptr;
    const auto* second_metric = second ? member(*second, metric) : nullptr;
    add_assertion(result, "descending_threshold_sort",
                  first_metric && second_metric && first_metric->is_number() &&
                      second_metric->is_number() &&
                      first_metric->as_number() >= second_metric->as_number(),
                  true,
                  first_metric && second_metric && first_metric->is_number() &&
                          second_metric->is_number()
                      ? Json(first_metric->as_number() >= second_metric->as_number())
                      : Json(nullptr));
    const auto* sources = member(document, "sources");
    const bool master = source_exists(document, universe == "high-price"
        ? "list/func_bygtj102_1.jsn" : "list/func_qyjlb102_1.jsn");
    bool detail = false, chart = false;
    bool healthy = sources && sources->is_array() && sources->size() == 3;
    if (sources && sources->is_array()) {
        for (const auto& source : sources->as_array()) {
            const auto* resource = member(source, "resource");
            if (resource && resource->is_string()) {
                detail = detail || resource->as_string().rfind("bygtj1/", 0) == 0;
                chart = chart || resource->as_string().rfind("bygtj3/", 0) == 0;
            }
            const auto* attempts = member(source, "attempts");
            const auto* stale = member(source, "stale");
            healthy = healthy && attempts && attempts->is_number() &&
                attempts->as_number() >= 1 && stale && stale->is_bool();
        }
    }
    add_assertion(result, "exact_sources", master && detail && chart,
                  true, master && detail && chart);
    add_assertion(result, "source_health", healthy, true, healthy);
}

void validate_capital_strength_ranking(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool ranking = contract_id == "capital-strength-ranking-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-capital-strength-native-v1"),
                  "tdx-market-capital-strength-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "view",
                  string_is(member(document, "view"),
                            ranking ? "ranking" : "confluence"),
                  ranking ? "ranking" : "confluence",
                  value_or_null(member(document, "view")));
    const auto* summary = member(document, "summary");
    if (ranking) {
        add_assertion(result, "period",
                      string_is(member(document, "period"), "5d"), "5d",
                      value_or_null(member(document, "period")));
        const auto* rows = summary ? member(*summary, "source_rows") : nullptr;
        const auto* names = summary ? member(*summary, "names_resolved") : nullptr;
        const auto* sorted = summary
            ? member(*summary, "source_ddx_descending") : nullptr;
        add_assertion(result, "top_100_population",
                      number_is(rows, 100) && names && names->is_number() &&
                          names->as_number() >= 95 && bool_is(sorted, true),
                      "100 rows, at least 95 names, source sorted by DDX",
                      value_or_null(summary));
        const auto* dates = summary ? member(*summary, "statistics_dates") : nullptr;
        const Json* date = dates && dates->is_array() && dates->size() == 1
            ? &dates->as_array().front() : nullptr;
        add_assertion(result, "statistics_date",
                      date && date->is_string() && date->as_string().size() == 8,
                      "one YYYYMMDD date", value_or_null(date));
    } else {
        const auto* source_rows = summary ? member(*summary, "source_rows") : nullptr;
        const auto* unique = summary ? member(*summary, "unique_securities") : nullptr;
        const auto* repeated = summary
            ? member(*summary, "at_least_two_periods") : nullptr;
        add_assertion(result, "five_period_population",
                      number_is(source_rows, 500) && unique && unique->is_number() &&
                          unique->as_number() >= 100 && unique->as_number() <= 500 &&
                          repeated && repeated->is_number() && repeated->as_number() >= 1,
                      "500 source rows with 100..500 securities and overlap",
                      value_or_null(summary));
    }
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    add_assertion(result, "record_identity",
                  security && security->is_object() &&
                      bool_is(member(*security, "name_resolved"), true),
                  "resolved security", value_or_null(security));
    if (ranking) {
        const auto* raw = first ? member(*first, "raw") : nullptr;
        const auto* first_ddx = first
            ? member(*first, "ddx_float_share_pct") : nullptr;
        const auto* second_ddx = second
            ? member(*second, "ddx_float_share_pct") : nullptr;
        const auto* total = first ? member(*first, "total_net_inflow_yuan") : nullptr;
        const auto* main = first ? member(*first, "main_net_inflow_yuan") : nullptr;
        add_assertion(result, "typed_money_and_raw",
                      raw && raw->is_object() && first_ddx && first_ddx->is_number() &&
                          total && total->is_number() && main && main->is_number(),
                      "DDX, yuan amounts, and raw row", value_or_null(first));
        add_assertion(result, "descending_ddx_sort",
                      first_ddx && second_ddx && first_ddx->is_number() &&
                          second_ddx->is_number() &&
                          first_ddx->as_number() >= second_ddx->as_number(),
                      true,
                      first_ddx && second_ddx && first_ddx->is_number() &&
                              second_ddx->is_number()
                          ? Json(first_ddx->as_number() >= second_ddx->as_number())
                          : Json(nullptr));
    } else {
        const auto* first_count = first ? member(*first, "period_count") : nullptr;
        const auto* second_count = second ? member(*second, "period_count") : nullptr;
        const auto* first_average = first
            ? member(*first, "average_ddx_float_share_pct") : nullptr;
        const auto* second_average = second
            ? member(*second, "average_ddx_float_share_pct") : nullptr;
        bool sorted = first_count && second_count && first_count->is_number() &&
            second_count->is_number() && first_count->as_number() >= second_count->as_number();
        if (sorted && first_count->as_number() == second_count->as_number())
            sorted = first_average && second_average && first_average->is_number() &&
                second_average->is_number() &&
                first_average->as_number() >= second_average->as_number();
        const auto* observations = first ? member(*first, "observations") : nullptr;
        add_assertion(result, "descending_confluence_sort", sorted, true, sorted);
        add_assertion(result, "observations_retained",
                      observations && observations->is_array() && first_count &&
                          first_count->is_number() &&
                          observations->size() == static_cast<std::size_t>(
                              first_count->as_number()),
                      "one observation per matched period",
                      value_or_null(observations));
    }
    const std::array<const char*, 5> expected_sources{{
        "list/func_qszj101_1.jsn", "list/func_qszj102_1.jsn",
        "list/func_qszj103_1.jsn", "list/func_qszj104_1.jsn",
        "list/func_qszj105_1.jsn"}};
    bool exact = true;
    if (ranking) exact = source_exists(document, expected_sources[0]);
    else for (const auto* resource : expected_sources)
        exact = exact && source_exists(document, resource);
    const auto* sources = member(document, "sources");
    bool healthy = sources && sources->is_array() &&
        sources->size() == (ranking ? 1 : expected_sources.size());
    if (sources && sources->is_array()) {
        for (const auto& source : sources->as_array()) {
            const auto* attempts = member(source, "attempts");
            const auto* stale = member(source, "stale");
            healthy = healthy && attempts && attempts->is_number() &&
                attempts->as_number() >= 1 && stale && stale->is_bool();
        }
    }
    add_assertion(result, "exact_sources", exact, true, exact);
    add_assertion(result, "source_health", healthy, true, healthy);
}

void validate_strong_stocks_intervals(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool detail = contract_id == "strong-stocks-detail-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-strong-stocks-native-v1"),
                  "tdx-market-strong-stocks-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "view",
                  string_is(member(document, "view"),
                            detail ? "detail" : "intervals"),
                  detail ? "detail" : "intervals",
                  value_or_null(member(document, "view")));
    const auto* summary = member(document, "summary");
    if (!detail) {
        const auto* rows = summary ? member(*summary, "source_rows") : nullptr;
        const auto* unique = summary ? member(*summary, "unique_securities") : nullptr;
        const auto* names = summary ? member(*summary, "names_resolved") : nullptr;
        const auto* first_date = summary ? member(*summary, "first_start_date") : nullptr;
        const auto* latest_date = summary ? member(*summary, "latest_end_date") : nullptr;
        const bool dates = first_date && latest_date && first_date->is_string() &&
            latest_date->is_string() && first_date->as_string().size() == 10 &&
            latest_date->as_string().size() == 10 &&
            first_date->as_string() <= latest_date->as_string();
        add_assertion(result, "rolling_population",
                      rows && rows->is_number() && rows->as_number() >= 500 &&
                          unique && unique->is_number() && unique->as_number() >= 450 &&
                          names && names->is_number() && names->as_number() >= 450 && dates,
                      "at least 500 intervals, 450 securities/names, valid date range",
                      value_or_null(summary));
    } else {
        const auto* days = summary ? member(*summary, "days") : nullptr;
        const auto* expected = summary ? member(*summary, "expected_trading_days") : nullptr;
        const auto* complete = summary ? member(*summary, "complete") : nullptr;
        const auto* reason_days = summary ? member(*summary, "reason_days") : nullptr;
        const auto* interval = summary ? member(*summary, "interval") : nullptr;
        add_assertion(result, "detail_completeness",
                      days && expected && days->is_number() && expected->is_number() &&
                          days->as_number() >= 1 && days->as_number() == expected->as_number() &&
                          bool_is(complete, true) && reason_days && reason_days->is_number() &&
                          reason_days->as_number() >= 1 && interval && interval->is_object(),
                      "complete interval with at least one reason day",
                      value_or_null(summary));
    }
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "record_identity",
                  security && security->is_object() &&
                      bool_is(member(*security, "name_resolved"), true) &&
                      raw && raw->is_object(),
                  "resolved security and raw row", value_or_null(security));
    if (!detail) {
        const auto* interval = first ? member(*first, "interval_id") : nullptr;
        const auto* days = first ? member(*first, "trading_days") : nullptr;
        const auto* boards = first ? member(*first, "limit_up_days") : nullptr;
        const auto* first_end = first ? member(*first, "end_date") : nullptr;
        const auto* second_end = second ? member(*second, "end_date") : nullptr;
        add_assertion(result, "typed_interval",
                      interval && interval->is_string() && interval->as_string().size() == 18 &&
                          days && days->is_number() && boards && boards->is_number(),
                      "18-digit id with parsed days/boards", value_or_null(first));
        add_assertion(result, "descending_end_date",
                      first_end && second_end && first_end->is_string() &&
                          second_end->is_string() &&
                          first_end->as_string() >= second_end->as_string(),
                      true,
                      first_end && second_end && first_end->is_string() &&
                              second_end->is_string()
                          ? Json(first_end->as_string() >= second_end->as_string())
                          : Json(nullptr));
    } else {
        const auto* first_date = first ? member(*first, "date") : nullptr;
        const auto* second_date = second ? member(*second, "date") : nullptr;
        const auto* reason = first ? member(*first, "limit_up_reason") : nullptr;
        const auto* amount = first ? member(*first, "turnover_amount_yuan") : nullptr;
        const auto* limit_up = first ? member(*first, "market_limit_up_count") : nullptr;
        const auto* broken = first ? member(*first, "market_broken_limit_count") : nullptr;
        const auto* seal = first ? member(*first, "market_seal_success_pct") : nullptr;
        add_assertion(result, "typed_daily_reason",
                      reason && reason->is_string() && !reason->as_string().empty() &&
                          amount && amount->is_number() && limit_up && limit_up->is_number() &&
                          broken && broken->is_number() && seal && seal->is_number(),
                      "reason, turnover and market temperature", value_or_null(first));
        add_assertion(result, "ascending_detail_date",
                      first_date && second_date && first_date->is_string() &&
                          second_date->is_string() &&
                          first_date->as_string() <= second_date->as_string(),
                      true,
                      first_date && second_date && first_date->is_string() &&
                              second_date->is_string()
                          ? Json(first_date->as_string() <= second_date->as_string())
                          : Json(nullptr));
    }
    const auto* sources = member(document, "sources");
    bool exact = source_exists(document, "list/func_ygzl101_1.jsn");
    bool has_detail = !detail;
    bool healthy = sources && sources->is_array() &&
        sources->size() == static_cast<std::size_t>(detail ? 2 : 1);
    if (sources && sources->is_array()) {
        for (const auto& source : sources->as_array()) {
            const auto* resource = member(source, "resource");
            if (detail && resource && resource->is_string() &&
                resource->as_string().rfind("ygzl/", 0) == 0) has_detail = true;
            const auto* attempts = member(source, "attempts");
            const auto* stale = member(source, "stale");
            healthy = healthy && attempts && attempts->is_number() &&
                attempts->as_number() >= 1 && stale && stale->is_bool();
        }
    }
    exact = exact && has_detail;
    add_assertion(result, "exact_sources", exact, true, exact);
    add_assertion(result, "source_health", healthy, true, healthy);
}

struct SignalContract {
    std::string_view id;
    SignalContractValidator validate;
};

constexpr std::array<SignalContract, 8> signal_contracts{{
    {"block-rotation-live", validate_block_rotation},
    {"limit-ladder-live", validate_limit_ladder},
    {"threshold-stocks-high-price-live", validate_threshold_stocks_high_price},
    {"threshold-stocks-mega-cap-live", validate_threshold_stocks_high_price},
    {"capital-strength-ranking-live", validate_capital_strength_ranking},
    {"capital-strength-confluence-live", validate_capital_strength_ranking},
    {"strong-stocks-intervals-live", validate_strong_stocks_intervals},
    {"strong-stocks-detail-live", validate_strong_stocks_intervals},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < signal_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < signal_contracts.size(); ++right)
            if (signal_contracts[left].id == signal_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_research_signal_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : signal_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail