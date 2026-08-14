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

// specialized-metrics-live
void validate_specialized_metrics(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-specialized-metrics-native-v1"),
                  "tdx-market-specialized-metrics-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* banks = member_path(document, {"summary", "banks"});
    const auto* brokers = member_path(document, {"summary", "securities"});
    const auto* insurers = member_path(document, {"summary", "insurers"});
    const auto* matched = member(document, "match_count");
    const bool population = banks && banks->is_number() && banks->as_number() >= 40 &&
        brokers && brokers->is_number() && brokers->as_number() >= 45 &&
        insurers && insurers->is_number() && insurers->as_number() >= 5 &&
        matched && matched->is_number() && banks->as_number() +
            brokers->as_number() + insurers->as_number() == matched->as_number();
    add_assertion(result, "population_reconciliation", population,
                  "at least 40 banks, 45 brokers and 5 insurers; counts reconcile",
                  population);
    bool bank_ratio = false, broker_yoy = false, insurer_ratio = false;
    bool raw_audit = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() &&
            member(row, "source_resource") && member(row, "source_resource")->is_string();
        if (!raw || !raw->is_object()) continue;
        if (string_is(member(row, "kind"), "banks")) {
            const auto source = numeric_value(member(*raw, "zbczl"));
            const auto normalized = numeric_value(member(row, "capital_adequacy_ratio_pct"));
            bank_ratio = bank_ratio || (source && normalized &&
                std::abs(*normalized - *source * 100.0) < 0.000001);
        } else if (string_is(member(row, "kind"), "securities")) {
            const auto current = numeric_value(member(*raw, "yysr"));
            const auto prior = numeric_value(member(*raw, "snyysr"));
            const auto normalized = numeric_value(member(row, "monthly_revenue_yoy_pct"));
            broker_yoy = broker_yoy || (current && prior && normalized && *prior != 0.0 &&
                std::abs(*normalized - (*current - *prior) * 100.0 / std::abs(*prior)) < 0.000001);
        } else if (string_is(member(row, "kind"), "insurers")) {
            const auto source = numeric_value(member(*raw, "T012"));
            const auto normalized = numeric_value(member(row, "core_solvency_adequacy_ratio_pct"));
            insurer_ratio = insurer_ratio || (source && normalized &&
                std::abs(*normalized - *source * 100.0) < 0.000001);
        }
    } else raw_audit = false;
    add_assertion(result, "cfg_units_and_formulas",
                  bank_ratio && broker_yoy && insurer_ratio,
                  "decimal ratios expose percent companions and broker YoY is derived from source months",
                  bank_ratio && broker_yoy && insurer_ratio);
    add_assertion(result, "raw_audit", raw_audit,
                  "all rows retain source resource and raw fields", raw_audit);
    const std::vector<std::string> required_sources{
        "list/func_hyjyfx101_1.jsn", "list/func_hyjyfx102_1.jsn",
        "list/func_hyjyfx103_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
}

// company-changes-live
void validate_company_changes(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-company-changes-native-v1"),
                  "tdx-market-company-changes-native-v1",
                  value_or_null(member(document, "schema")));
    const std::vector<std::pair<std::string, double>> minimums{
        {"security-renames", 300}, {"company-renames", 80},
        {"mainland-index", 1000}, {"major-equity", 200},
        {"hk-index", 100}, {"controllers", 100},
        {"industries", 1000}, {"equity-transfers", 600},
        {"neeq-index", 1700}};
    double sum = 0.0;
    bool population = true;
    for (const auto& [kind, minimum] : minimums) {
        const auto* value = member_path(document, {"summary", kind});
        population = population && value && value->is_number() &&
            value->as_number() >= minimum;
        if (value && value->is_number()) sum += value->as_number();
    }
    const auto* matched = member(document, "match_count");
    population = population && matched && matched->is_number() &&
        std::abs(sum - matched->as_number()) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "all nine families meet minimum populations and reconcile",
                  population);
    bool shares_unit = false, transfer_price = false, blank_neeq_market = false;
    bool raw_audit = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() &&
            member(row, "source_resource") && member(row, "source_resource")->is_string();
        if (!raw || !raw->is_object()) continue;
        if (string_is(member(row, "kind"), "major-equity")) {
            const auto source = numeric_value(member(*raw, "NBD3"));
            const auto normalized = numeric_value(member(row, "shares"));
            shares_unit = shares_unit || (source && normalized &&
                std::abs(*normalized - *source * 10000.0) < 0.01);
        } else if (string_is(member(row, "kind"), "equity-transfers")) {
            const auto amount = numeric_value(member(*raw, "zkx"));
            const auto shares = numeric_value(member(*raw, "zrgb"));
            const auto normalized = numeric_value(member(row, "transfer_price_yuan"));
            transfer_price = transfer_price || (amount && shares && normalized &&
                *shares != 0.0 && std::abs(*normalized - *amount / *shares) < 0.000001);
        } else if (string_is(member(row, "kind"), "neeq-index") &&
                   string_is(member(*raw, "$SC"), "")) {
            blank_neeq_market = blank_neeq_market ||
                string_is(member_path(row, {"security", "market"}), "neeq");
        }
    } else raw_audit = false;
    add_assertion(result, "cfg_units_and_fallbacks",
                  shares_unit && transfer_price && blank_neeq_market,
                  "ten-thousand shares, transfer price and blank NEEQ host market are normalized",
                  shares_unit && transfer_price && blank_neeq_market);
    add_assertion(result, "raw_audit", raw_audit,
                  "all records retain source resource and raw fields", raw_audit);
    const std::vector<std::string> required_sources{
        "list/func_zqbg101_1.jsn", "list/func_zqbg102_1.jsn",
        "list/func_zqbg103_1.jsn", "list/func_zqbg104_1.jsn",
        "list/func_zqbg105_1.jsn", "list/func_zqbg107_1.jsn",
        "list/func_zqbg108_1.jsn", "list/func_zqbg109_1.jsn",
        "list/func_zqbg110_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
}

// financial-screen-live
void validate_financial_screen(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-financial-screen-native-v1"),
                  "tdx-market-financial-screen-native-v1",
                  value_or_null(member(document, "schema")));
    const std::vector<std::pair<std::string, double>> minimums{
        {"sh-main", 1600}, {"sz-main", 1400}, {"chinext", 1300},
        {"star", 550}, {"beijing", 300}};
    double sum = 0.0;
    bool population = true;
    for (const auto& [board, minimum] : minimums) {
        const auto* value = member_path(document, {"summary", board});
        population = population && value && value->is_number() &&
            value->as_number() >= minimum;
        if (value && value->is_number()) sum += value->as_number();
    }
    const auto* matched = member(document, "match_count");
    population = population && matched && matched->is_number() &&
        std::abs(sum - matched->as_number()) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "five active boards meet minimum populations and reconcile",
                  population);
    bool cap_unit = false, percent_unit = false, liability_yoy = false;
    bool raw_audit = true, descending = true;
    std::optional<double> previous_cap;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() &&
            member(row, "source_resource") && member(row, "source_resource")->is_string();
        const auto cap = numeric_value(member(row, "market_cap_yuan"));
        if (cap) {
            if (previous_cap && *cap > *previous_cap + 0.01) descending = false;
            previous_cap = cap;
        }
        if (!raw || !raw->is_object()) continue;
        const auto source_cap = numeric_value(member(*raw, "SZ"));
        cap_unit = cap_unit || (source_cap && cap &&
            std::abs(*cap - *source_cap * 10000000.0) < 0.01);
        const auto source_debt = numeric_value(member(*raw, "ZCFZ"));
        const auto debt = numeric_value(member(row, "debt_ratio_pct"));
        percent_unit = percent_unit || (source_debt && debt &&
            std::abs(*source_debt - *debt) < 0.000001);
        const auto current = numeric_value(member(*raw, "htfzbq"));
        const auto prior = numeric_value(member(*raw, "htfzsq"));
        const auto yoy = numeric_value(member(row, "contract_liability_yoy_pct"));
        liability_yoy = liability_yoy || (current && prior && yoy && *prior != 0.0 &&
            std::abs(*yoy - (*current - *prior) * 100.0 / *prior) < 0.000001);
    } else raw_audit = false;
    add_assertion(result, "cfg_units_and_formula",
                  cap_unit && percent_unit && liability_yoy,
                  "market cap, percentage points and contract-liability growth match CFG semantics",
                  cap_unit && percent_unit && liability_yoy);
    add_assertion(result, "descending_market_cap", descending,
                  true, descending);
    add_assertion(result, "raw_audit", raw_audit,
                  "all records retain source resource and raw fields", raw_audit);
    const std::vector<std::string> required_sources{
        "list/func_cwzb101_1.jsn", "list/func_cwzb102_1.jsn",
        "list/func_cwzb104_1.jsn", "list/func_cwzb105_1.jsn",
        "list/func_cwzb107_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
}

// small-cap-growth-live
void validate_small_cap_growth(const Json& document, Json& result) {
    add_assertion(result, "schema_dataset",
                  string_is(member(document, "schema"),
                            "tdx-market-financial-screen-native-v1") &&
                      string_is(member(document, "dataset"), "small-cap-growth"),
                  "tdx-market-financial-screen-native-v1/small-cap-growth",
                  value_or_null(member(document, "dataset")));
    const auto matched = numeric_value(member(document, "match_count"));
    const auto sh = numeric_value(member_path(document, {"summary", "sh-main"}));
    const auto sz = numeric_value(member_path(document, {"summary", "sz-main"}));
    const auto chinext = numeric_value(member_path(document, {"summary", "chinext"}));
    const auto star = numeric_value(member_path(document, {"summary", "star"}));
    const bool population = matched && sh && sz && chinext && star &&
        *matched >= 30 && *sh + *sz >= 12 && *chinext >= 5 && *star >= 10 &&
        std::abs(*sh + *sz + *chinext + *star - *matched) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "30+ unique master rows across main board, ChiNext and STAR", population);
    const auto* rows = member(document, "records");
    bool typed = rows && rows->is_array() && rows->size() >= 30;
    bool units = true, descending = true;
    std::optional<double> previous;
    if (typed) for (const auto& row : rows->as_array()) {
        const auto* security = member(row, "security");
        const auto* raw = member(row, "raw");
        const auto profit = numeric_value(member(row, "adjusted_net_profit_yoy_pct"));
        const auto profit_t1 = numeric_value(member(row, "adjusted_net_profit_yoy_t_minus_1_pct"));
        const auto profit_t2 = numeric_value(member(row, "adjusted_net_profit_yoy_t_minus_2_pct"));
        const auto profit_t3 = numeric_value(member(row, "adjusted_net_profit_yoy_t_minus_3_pct"));
        const auto profit_cagr = numeric_value(member(row, "adjusted_net_profit_cagr_3y_pct"));
        const auto revenue = numeric_value(member(row, "revenue_yoy_pct"));
        const auto revenue_cagr = numeric_value(member(row, "revenue_cagr_3y_pct"));
        typed = typed && string_is(member(row, "dataset"), "small-cap-growth") &&
            security && security->is_object() &&
            nonempty_string(member(*security, "code")) &&
            nonempty_string(member(row, "report_period")) && profit && profit_t1 &&
            profit_t2 && profit_t3 && profit_cagr && revenue && revenue_cagr &&
            raw && raw->is_object();
        if (profit_cagr) {
            if (previous && *profit_cagr > *previous + 0.000001) descending = false;
            previous = profit_cagr;
        }
        if (raw && raw->is_object()) {
            const auto raw_profit = numeric_value(member(*raw, "jlzs1"));
            const auto raw_profit_cagr = numeric_value(member(*raw, "jlfh"));
            const auto raw_revenue = numeric_value(member(*raw, "yszs1"));
            const auto raw_revenue_cagr = numeric_value(member(*raw, "ysfh"));
            units = units && profit && profit_cagr && revenue && revenue_cagr &&
                raw_profit && raw_profit_cagr && raw_revenue && raw_revenue_cagr &&
                std::abs(*profit - *raw_profit) < 0.000001 &&
                std::abs(*profit_cagr - *raw_profit_cagr) < 0.000001 &&
                std::abs(*revenue - *raw_revenue) < 0.000001 &&
                std::abs(*revenue_cagr - *raw_revenue_cagr) < 0.000001;
        } else units = false;
    }
    add_assertion(result, "typed_four_period_growth", typed,
                  "T/T-1/T-2/T-3 profit growth plus profit/revenue CAGR", typed);
    add_assertion(result, "cfg_percentage_points", units,
                  "jlzs1/jlfh/yszs1/ysfh retained as percentage points", units);
    add_assertion(result, "descending_profit_cagr", descending, true, descending);
    const auto* chinext_exact = member_path(
        document, {"projection_reconciliation", "chinext", "exact_match"});
    const auto* star_exact = member_path(
        document, {"projection_reconciliation", "star", "exact_match"});
    const auto master_unique = numeric_value(member_path(
        document, {"projection_reconciliation", "master_unique_count"}));
    const bool projections = chinext_exact && chinext_exact->is_bool() &&
        chinext_exact->as_bool() && star_exact && star_exact->is_bool() &&
        star_exact->as_bool() && master_unique && matched &&
        std::abs(*master_unique - *matched) < 0.5;
    add_assertion(result, "board_projection_reconciliation", projections,
                  "XPCZ103/104 exactly match 101 ChiNext/STAR subsets", projections);
    const std::vector<std::string> required_sources{
        "list/func_xpcz101_1.jsn", "list/func_xpcz103_1.jsn",
        "list/func_xpcz104_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() && sources->size() == 3;
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", exact_sources, 3, exact_sources);
}

// equity-performance-live
void validate_equity_performance(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-equity-performance-native-v1"),
                  "tdx-market-equity-performance-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* sh = member_path(document, {"summary", "sh"});
    const auto* sz = member_path(document, {"summary", "sz"});
    const auto* bj = member_path(document, {"summary", "bj"});
    const auto* matched = member(document, "match_count");
    const bool population = sh && sh->is_number() && sh->as_number() >= 2200 &&
        sz && sz->is_number() && sz->as_number() >= 2800 &&
        bj && bj->is_number() && bj->as_number() >= 300 &&
        matched && matched->is_number() && matched->as_number() >= 5300 &&
        std::abs(sh->as_number() + sz->as_number() + bj->as_number() -
                 matched->as_number()) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "three A-share markets meet minimum populations and reconcile",
                  population);
    bool month_formula = true, turnover_unit = true, percent_unit = true;
    bool month_observed = false, turnover_observed = false, percent_observed = false;
    bool raw_audit = true, descending = true;
    std::optional<double> previous_return;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto value = numeric_value(member(row, "return_5d_pct"));
        if (value) {
            if (previous_return && *value > *previous_return + 0.000001)
                descending = false;
            previous_return = value;
        }
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() &&
            string_is(member(row, "source_resource"), "list/func_aghq101.jsn");
        if (!raw || !raw->is_object()) continue;
        const auto close = numeric_value(member(*raw, "price0"));
        const auto prior = numeric_value(member(*raw, "price1"));
        const auto month = numeric_value(member(row, "month_to_date_pct"));
        if (close && prior && month && *prior != 0.0) {
            month_observed = true;
            month_formula = month_formula &&
                std::abs(*month - (*close - *prior) * 100.0 / *prior) < 0.000001;
        }
        const auto source_turnover = numeric_value(member(*raw, "cje"));
        const auto turnover = numeric_value(member(row, "daily_turnover_yuan"));
        if (source_turnover && turnover) {
            turnover_observed = true;
            turnover_unit = turnover_unit &&
                std::abs(*turnover - *source_turnover) < 0.01;
        }
        const auto source_return = numeric_value(member(*raw, "zdf_5d"));
        if (source_return && value) {
            percent_observed = true;
            percent_unit = percent_unit &&
                std::abs(*value - *source_return) < 0.000001;
        }
    } else raw_audit = false;
    add_assertion(result, "cfg_units_and_formula",
                  month_observed && turnover_observed && percent_observed &&
                      month_formula && turnover_unit && percent_unit,
                  "month return formula, yuan turnover and percentage points match CFG",
                  month_observed && turnover_observed && percent_observed &&
                      month_formula && turnover_unit && percent_unit);
    add_assertion(result, "descending_return_5d", descending, true, descending);
    add_assertion(result, "raw_audit", raw_audit,
                  "all records retain AGHQ source and raw fields", raw_audit);
    const auto* sources = member(document, "sources");
    const bool exact_sources = sources && sources->is_array() &&
        sources->size() == 1 && source_exists(document, "list/func_aghq101.jsn");
    add_assertion(result, "exact_sources", exact_sources, 1, exact_sources);
}

// corporate-orders-live
void validate_corporate_orders(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-corporate-orders-native-v1"),
                  "tdx-market-corporate-orders-native-v1",
                  value_or_null(member(document, "schema")));
    const auto tenders = numeric_value(member_path(document, {"summary", "tenders"}));
    const auto contracts = numeric_value(
        member_path(document, {"summary", "major_contracts"}));
    const auto matched = numeric_value(member(document, "match_count"));
    const auto mismatches = numeric_value(
        member_path(document, {"summary", "formula_mismatches"}));
    const bool population = tenders && contracts && matched && mismatches &&
        *tenders >= 5700 && *contracts >= 39 &&
        std::abs(*tenders + *contracts - *matched) < 0.5 && *mismatches == 0;
    add_assertion(result, "population_and_formula", population,
                  "5700+ tender rows, 39 contracts and zero revenue-share mismatches",
                  population);
    bool auditable = true, link_observed = false;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        auditable = auditable && member(row, "raw") && member(row, "raw")->is_object() &&
            member(row, "security") && member(row, "security")->is_object();
        const auto* url = member(row, "source_url");
        link_observed = link_observed ||
            (url && url->is_string() && !url->as_string().empty());
    } else auditable = false;
    add_assertion(result, "raw_and_links", auditable && link_observed,
                  "normalized securities retain raw rows and an announcement URL",
                  auditable && link_observed);
    const bool sources_ok = source_exists(document, "list/func_zb101_1.jsn") &&
        source_exists(document, "list/func_zdht101_1.jsn");
    add_assertion(result, "exact_sources", sources_ok, 2, sources_ok);
}

// event-impact-live
void validate_event_impact(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-event-impact-native-v1"),
                  "tdx-market-event-impact-native-v1",
                  value_or_null(member(document, "schema")));
    const auto sh = numeric_value(
        member_path(document, {"summary", "shanghai_composite"}));
    const auto hk = numeric_value(member_path(document, {"summary", "hang_seng"}));
    const auto nasdaq = numeric_value(
        member_path(document, {"summary", "nasdaq_composite"}));
    const auto matched = numeric_value(member(document, "match_count"));
    const bool population = sh && hk && nasdaq && matched && *sh >= 116 &&
        *hk >= 232 && *nasdaq >= 235 &&
        std::abs(*sh + *hk + *nasdaq - *matched) < 0.5;
    add_assertion(result, "benchmark_population", population,
                  "Shanghai, Hang Seng and Nasdaq event rows reconcile", population);
    bool shape = true;
    const auto* events = member(document, "records");
    if (events && events->is_array()) for (const auto& row : events->as_array()) {
        shape = shape && member(row, "benchmark") && member(row, "description") &&
            member(row, "closes") && member(row, "impacts") &&
            member(row, "raw") && member(row, "raw")->is_object();
    } else shape = false;
    add_assertion(result, "typed_window", shape,
                  "all events retain benchmark, dates, return window and raw evidence",
                  shape);
    const bool sources_ok = source_exists(document, "list/func_zdsj101_1.jsn") &&
        source_exists(document, "list/func_zdsj102_1.jsn") &&
        source_exists(document, "list/func_zdsj103_1.jsn");
    add_assertion(result, "exact_sources", sources_ok, 3, sources_ok);
}

// global-performance-live
void validate_global_performance(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-global-performance-native-v1"),
                  "tdx-market-global-performance-native-v1",
                  value_or_null(member(document, "schema")));
    const auto indices = numeric_value(
        member_path(document, {"summary", "major_indices"}));
    const auto overseas = numeric_value(
        member_path(document, {"summary", "overseas_china"}));
    const auto matched = numeric_value(member(document, "match_count"));
    const bool population = indices && overseas && matched && *indices == 13 &&
        *overseas >= 350 && std::abs(*indices + *overseas - *matched) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "13 major indices plus 350+ overseas China securities", population);
    bool formula_observed = false, formula_ok = true, auditable = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        auditable = auditable && raw && raw->is_object() &&
            member(row, "instrument") && member(row, "instrument")->is_object();
        if (!raw || !raw->is_object()) continue;
        const auto close = numeric_value(member(*raw, "price0"));
        const auto reference = numeric_value(member(*raw,
            string_is(member(row, "kind"), "major-index") ? "price5" : "price6"));
        const auto value = numeric_value(member(row, "return_5d_pct"));
        if (close && reference && value && std::abs(*reference) > 0.000001) {
            formula_observed = true;
            formula_ok = formula_ok &&
                std::abs(*value - (*close - *reference) * 100.0 / *reference) < 0.000001;
        }
    } else auditable = false;
    add_assertion(result, "return_formula_and_raw",
                  formula_observed && formula_ok && auditable,
                  "5-day return is reproduced from reference close and raw is retained",
                  formula_observed && formula_ok && auditable);
    const bool sources_ok = source_exists(document, "list/func_zyzh101.jsn") &&
        source_exists(document, "list/func_zgghq101.jsn");
    add_assertion(result, "exact_sources", sources_ok, 2, sources_ok);
}

struct CompanyContract {
    std::string_view id;
    MarketCorporateCatalogContractValidator validate;
};

constexpr std::array<CompanyContract, 8> company_contracts{{
    {"specialized-metrics-live", validate_specialized_metrics},
    {"company-changes-live", validate_company_changes},
    {"financial-screen-live", validate_financial_screen},
    {"small-cap-growth-live", validate_small_cap_growth},
    {"equity-performance-live", validate_equity_performance},
    {"corporate-orders-live", validate_corporate_orders},
    {"event-impact-live", validate_event_impact},
    {"global-performance-live", validate_global_performance},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < company_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < company_contracts.size(); ++right)
            if (company_contracts[left].id == company_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_corporate_company_contract(
    const std::string& contract_id, const Json& document,
    const Json&, Json& result) {
    for (const auto& contract : company_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
