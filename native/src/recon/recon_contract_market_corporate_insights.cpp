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

// futures-calendar-live
void validate_futures_calendar(const Json& document, Json& result) {
    const auto matched = numeric_value(member(document, "match_count"));
    bool typed = string_is(member(document, "schema"),
                           "tdx-market-calendar-native-v1") &&
        string_is(member(document, "view"), "futures") && matched && *matched >= 20;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) for (const auto& row : rows->as_array())
        typed = typed && string_is(member(row, "kind"), "futures-calendar") &&
            member(row, "exchange_code") && member(row, "content") &&
            member(row, "raw") && member(row, "raw")->is_object();
    else typed = false;
    add_assertion(result, "typed_futures_calendar", typed,
                  "20+ exchange calendar rows retain content and raw evidence", typed);
    const bool source_ok = source_exists(document, "list/func_qhrl400_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_qhrl400_1.jsn", source_ok);
}

// institution-social-security-summary-live
void validate_institution_social_security_summary(const Json& document, Json& result) {
    const auto matched = numeric_value(member_path(document, {"counts", "matched"}));
    const auto* sections = member(document, "sections");
    bool typed = string_is(member(document, "schema"),
                           "tdx-institution-analysis-native-v1") &&
        string_is(member(document, "view"), "social-security-summary") &&
        matched && *matched >= 10 && sections && sections->is_array() &&
        sections->size() == 1;
    if (typed) {
        const auto& section = sections->as_array().front();
        typed = string_is(member(section, "layout"), "social-security-summary");
        const auto* rows = member(section, "records");
        if (rows && rows->is_array()) {
            for (const auto& row : rows->as_array())
                typed = typed && member_path(row, {"data", "holding_shares"}) &&
                    member_path(row, {"data", "total_share_pct"}) &&
                    member(row, "raw") && member(row, "raw")->is_object();
        } else typed = false;
    }
    add_assertion(result, "typed_social_security_summary", typed,
                  "10+ social-security summaries retain shares, ratio and raw",
                  typed);
    const bool source_ok = source_exists(document, "list/func_sbltgd101_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_sbltgd101_1.jsn", source_ok);
}

// institution-development-bank-holdings-live
void validate_institution_development_bank_holdings(const Json& document, Json& result) {
    const auto matched = numeric_value(member_path(document, {"counts", "matched"}));
    const auto* sections = member(document, "sections");
    bool typed = string_is(member(document, "schema"),
                           "tdx-institution-analysis-native-v1") &&
        string_is(member(document, "view"), "development-bank-holdings") &&
        matched && *matched == 5 && sections && sections->is_array() &&
        sections->size() == 1;
    if (typed) {
        const auto& section = sections->as_array().front();
        typed = string_is(member(section, "layout"),
                          "development-bank-holdings");
        const auto* rows = member(section, "records");
        if (rows && rows->is_array() && rows->size() == 5) {
            for (const auto& row : rows->as_array()) {
                const auto* date = member_path(row, {"data", "report_date"});
                const auto* position = member_path(
                    row, {"data", "shareholder_position"});
                const auto* detail = member_path(row, {"data", "holding_detail"});
                typed = typed && date && date->is_string() &&
                    date->as_string().size() == 8 && position &&
                    position->is_string() && !position->as_string().empty() &&
                    detail && detail->is_string() && !detail->as_string().empty() &&
                    member(row, "raw") && member(row, "raw")->is_object();
            }
        } else typed = false;
    }
    add_assertion(result, "typed_disclosures", typed,
                  "five dated holdings retain shareholder position, detail and raw", typed);
    const bool source_ok = source_exists(document, "list/func_tzcg105_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_tzcg105_1.jsn", source_ok);
}

// patent-statistics-live
void validate_patent_statistics(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-patent-statistics-native-v1"),
                  "tdx-market-patent-statistics-native-v1",
                  value_or_null(member(document, "schema")));
    const auto matched = numeric_value(member(document, "match_count"));
    const auto unique = numeric_value(
        member_path(document, {"summary", "unique_securities"}));
    const auto sz = numeric_value(member_path(document, {"summary", "sz"}));
    const auto sh = numeric_value(member_path(document, {"summary", "sh"}));
    const auto bj = numeric_value(member_path(document, {"summary", "bj"}));
    const bool population = matched && unique && sz && sh && bj &&
        *matched >= 3970 && *unique >= 3970 &&
        std::abs(*sz + *sh + *bj - *matched) < 0.5;
    add_assertion(result, "population", population,
                  "3970+ unique SZ/SH/BJ securities reconcile", population);
    bool typed = true, delta_observed = false, delta_ok = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        typed = typed && member(row, "security") &&
            member(row, "security")->is_object() &&
            member(row, "report_date") && raw && raw->is_object();
        const auto total = numeric_value(member(row, "cumulative_grant_total"));
        const auto classified = numeric_value(
            member(row, "cumulative_grant_classified_total"));
        const auto delta = numeric_value(
            member(row, "cumulative_grant_total_delta"));
        if (total && classified && delta) {
            delta_observed = true;
            delta_ok = delta_ok &&
                std::abs(*delta - (*total - *classified)) < 0.000001;
        }
    } else typed = false;
    add_assertion(result, "typed_and_auditable",
                  typed && delta_observed && delta_ok,
                  "records retain security/raw and source-total delta is reproducible",
                  typed && delta_observed && delta_ok);
    const bool source_ok = source_exists(document, "list/func_gszl101_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_gszl101_1.jsn", source_ok);
}

// overview-factors-live
void validate_overview_factors(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-overview-factors-native-v1"),
                  "tdx-market-overview-factors-native-v1",
                  value_or_null(member(document, "schema")));
    const auto matched = numeric_value(member(document, "match_count"));
    const auto positive = numeric_value(member_path(document, {"summary", "positive"}));
    const auto neutral = numeric_value(member_path(document, {"summary", "neutral"}));
    const auto negative = numeric_value(member_path(document, {"summary", "negative"}));
    const auto unrated = numeric_value(member_path(document, {"summary", "unrated"}));
    const bool population = matched && positive && neutral && negative && unrated &&
        *matched == 17 && *positive > 0 && *neutral > 0 &&
        *negative > 0 && *unrated > 0 &&
        *positive + *neutral + *negative + *unrated == *matched;
    add_assertion(result, "signal_population", population,
                  "17 factors reconcile across four nonempty rolling signal classes",
                  population);
    bool typed = true;
    std::map<std::string, std::size_t> observed;
    const auto* records = member(document, "records");
    if (records && records->is_array() && records->size() == 17) {
        for (const auto& row : records->as_array()) {
            const auto* signal = member(row, "signal");
            const bool valid_signal = signal && signal->is_string() &&
                (signal->as_string() == "positive" ||
                 signal->as_string() == "neutral" ||
                 signal->as_string() == "negative" ||
                 signal->as_string() == "unrated");
            typed = typed && member(row, "factor_id") &&
                member(row, "name") && member(row, "description") &&
                member(row, "chart_indicator") && valid_signal &&
                member(row, "raw") && member(row, "raw")->is_object();
            if (valid_signal) ++observed[signal->as_string()];
        }
    } else typed = false;
    add_assertion(result, "typed_snapshot", typed,
                  "all factors retain id/name/description/indicator/signal/raw", typed);
    const bool record_reconciliation = population &&
        observed["positive"] == static_cast<std::size_t>(*positive) &&
        observed["neutral"] == static_cast<std::size_t>(*neutral) &&
        observed["negative"] == static_cast<std::size_t>(*negative) &&
        observed["unrated"] == static_cast<std::size_t>(*unrated);
    add_assertion(result, "record_signal_reconciliation", record_reconciliation,
                  "record signals exactly reproduce the rolling summary", record_reconciliation);
    const bool source_ok = source_exists(document, "list/func_dpfx101_1.jsn");
    add_assertion(result, "source", source_ok,
                  "list/func_dpfx101_1.jsn", source_ok);
}

// notable-investor-holdings-live
void validate_notable_investor_holdings(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-shareholder-signals-native-v1"),
                  "tdx-market-shareholder-signals-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* selected = member(document, "selected_investor");
    const auto* directory = member(document, "investor_directory");
    const auto* holdings = member(document, "investor_holdings");
    const bool identity = string_is(member(document, "mode"), "investor") &&
        string_is(member(document, "view"), "investor-directory") &&
        selected && selected->is_object() &&
        string_is(member(*selected, "investor_id"), "GD012270") &&
        directory && directory->is_array() && directory->size() >= 1700 &&
        holdings && holdings->is_array() && holdings->size() == 35;
    add_assertion(result, "investor_directory_and_identity", identity,
                  "GD012270 selected from 1700+ investors with 35 holdings", identity);
    const bool reconciliation = bool_is(member_path(document,
            {"investor_reconciliation", "company_count_matches"}), true) &&
        bool_is(member_path(document,
            {"investor_reconciliation", "holding_shares_match"}), true) &&
        bool_is(member_path(document,
            {"investor_reconciliation", "holding_value_matches"}), true) &&
        number_is(member_path(document,
            {"investor_reconciliation", "actual_company_count"}), 35);
    add_assertion(result, "master_detail_reconciliation", reconciliation,
                  "company count, shares and value exactly reconcile to the directory", reconciliation);
    bool typed = holdings && holdings->is_array();
    bool formulas = true;
    if (holdings && holdings->is_array()) for (const auto& row : holdings->as_array()) {
        const auto* raw = member(row, "raw");
        const auto* security = member(row, "security");
        typed = typed && string_is(member(row, "kind"), "investor-holding") &&
            string_is(member(row, "investor_id"), "GD012270") &&
            string_is(member(row, "source_resource"), "nscg/GD012270.jsn") &&
            security && security->is_object() && raw && raw->is_object();
        if (!raw || !raw->is_object()) { formulas = false; continue; }
        const auto current_shares = numeric_value(member(*raw, "bqcg"));
        const auto prior_shares = numeric_value(member(*raw, "sqcg"));
        const auto share_change = numeric_value(member(row, "holding_change_shares"));
        const auto current_value = numeric_value(member(*raw, "bqje"));
        const auto prior_value = numeric_value(member(*raw, "sqje"));
        const auto value_change = numeric_value(member(row, "holding_value_change_yuan"));
        const auto current_pct = numeric_value(member(*raw, "bqbl"));
        const auto prior_pct = numeric_value(member(*raw, "sqbl"));
        const auto pct_change = numeric_value(member(row, "holding_pct_change"));
        const bool comparable = current_shares && prior_shares && share_change &&
            current_value && prior_value && value_change && current_pct && prior_pct &&
            pct_change && std::abs(*share_change - (*current_shares - *prior_shares)) < 0.01 &&
            std::abs(*value_change - (*current_value - *prior_value)) < 0.01 &&
            std::abs(*pct_change - (*current_pct - *prior_pct)) < 0.000001;
        const bool no_prior = current_shares && current_value && current_pct &&
            !prior_shares && !prior_value && !prior_pct && !share_change &&
            !value_change && !pct_change;
        formulas = formulas && (comparable || no_prior);
    }
    add_assertion(result, "typed_holdings_and_cfg_formulas", typed && formulas,
                  "all holdings retain security/raw and current-minus-prior formulas", typed && formulas);
    const bool sources_ok = source_exists(document, "list/func_nscg101_1.jsn") &&
        string_is(member_path(document, {"investor_detail_source", "resource"}),
                  "nscg/GD012270.jsn") &&
        number_is(member_path(document,
            {"investor_detail_source", "normalized_row_count"}), 35);
    add_assertion(result, "directory_and_detail_sources", sources_ok,
                  "directory plus dynamic nscg/GD012270.jsn", sources_ok);
}

// shareholder-signals-live
void validate_shareholder_signals(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-shareholder-signals-native-v1"),
                  "tdx-market-shareholder-signals-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* notable = member_path(document, {"summary", "notable-investors"});
    const auto* institution = member_path(document, {"summary", "institution-accumulation"});
    const auto* small_cap = member_path(document, {"summary", "small-cap-institution"});
    const auto* research = member_path(document, {"summary", "research-growth"});
    const auto* matched = member(document, "match_count");
    const bool population = notable && notable->is_number() && notable->as_number() >= 850 &&
        institution && institution->is_number() && institution->as_number() >= 40 &&
        small_cap && small_cap->is_number() && small_cap->as_number() >= 40 &&
        research && research->is_number() && research->as_number() >= 75 &&
        matched && matched->is_number() && std::abs(notable->as_number() +
            institution->as_number() + small_cap->as_number() +
            research->as_number() - matched->as_number()) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "four signal families meet minimum populations and reconcile",
                  population);
    bool notable_formula = false, institution_units = false, research_units = false;
    bool small_cap_formulas = false;
    bool raw_audit = true, descending = true;
    std::optional<double> previous_signal;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto signal = numeric_value(member(row, "signal_value"));
        if (signal) {
            if (previous_signal && *signal > *previous_signal + 0.000001)
                descending = false;
            previous_signal = signal;
        }
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() &&
            member(row, "source_resource") && member(row, "source_resource")->is_string();
        if (!raw || !raw->is_object()) continue;
        if (string_is(member(row, "kind"), "notable-investors")) {
            const auto shares = numeric_value(member(*raw, "N003"));
            const auto total = numeric_value(member(*raw, "N006"));
            const auto close = numeric_value(member(*raw, "N007"));
            const auto pct = numeric_value(member(row, "holding_pct"));
            const auto value = numeric_value(member(row, "holding_value_yuan"));
            notable_formula = notable_formula || (shares && total && close && pct && value &&
                *total != 0.0 && std::abs(*pct - *shares * 100.0 / *total) < 0.000001 &&
                std::abs(*value - *shares * *close) < 0.01);
        } else if (string_is(member(row, "kind"), "institution-accumulation")) {
            const auto growth = numeric_value(member(*raw, "cgzz"));
            const auto holder = numeric_value(member(*raw, "rsjs"));
            const auto growth_pct = numeric_value(member(row, "institution_holding_growth_pct"));
            const auto holder_pct = numeric_value(member(row, "shareholder_count_change_pct"));
            institution_units = institution_units || (growth && holder && growth_pct && holder_pct &&
                std::abs(*growth_pct - *growth * 100.0) < 0.000001 &&
                std::abs(*holder_pct - *holder * 100.0) < 0.000001);
        } else if (string_is(member(row, "kind"), "research-growth")) {
            const auto source_profit = numeric_value(member(*raw, "jlrzf"));
            const auto source_return = numeric_value(member(*raw, "bnzaf"));
            const auto profit = numeric_value(member(row, "net_profit_growth_pct"));
            const auto period_return = numeric_value(member(row, "return_6m_pct"));
            research_units = research_units || (source_profit && source_return && profit &&
                period_return && std::abs(*source_profit - *profit) < 0.000001 &&
                std::abs(*source_return - *period_return) < 0.000001);
        } else if (string_is(member(row, "kind"), "small-cap-institution")) {
            const auto shares = numeric_value(member(*raw, "zxcg"));
            const auto share_change = numeric_value(member(*raw, "cgbd"));
            const auto value = numeric_value(member(*raw, "ccsz"));
            const auto value_change = numeric_value(member(*raw, "zcsz"));
            const auto count = numeric_value(member(*raw, "jgsl"));
            const auto count_change = numeric_value(member(*raw, "jgbhl"));
            const auto share_pct = numeric_value(member(row, "institution_holding_growth_pct"));
            const auto value_pct = numeric_value(member(row, "institution_holding_value_change_pct"));
            const auto count_pct = numeric_value(member(row, "institution_count_change_pct"));
            small_cap_formulas = small_cap_formulas || (shares && share_change &&
                value && value_change && count && count_change && share_pct &&
                value_pct && count_pct && *shares != 0.0 && *value != 0.0 &&
                *count != 0.0 &&
                std::abs(*share_pct - *share_change * 100.0 / *shares) < 0.000001 &&
                std::abs(*value_pct - *value_change * 100.0 / *value) < 0.000001 &&
                std::abs(*count_pct - *count_change * 100.0 / *count) < 0.000001);
        }
    } else raw_audit = false;
    add_assertion(result, "cfg_units_and_formulas",
                  notable_formula && institution_units && research_units &&
                      small_cap_formulas,
                  "holding formulas, source ratios, XSZGP formulas and direct percentages match CFG",
                  notable_formula && institution_units && research_units &&
                      small_cap_formulas);
    add_assertion(result, "descending_signal", descending, true, descending);
    add_assertion(result, "raw_audit", raw_audit,
                  "all records retain source resource and raw fields", raw_audit);
    const std::vector<std::string> required_sources{
        "list/func_cwnscg101_1.jsn", "list/func_jgxc101_1.jsn",
        "list/func_jgzd101_1.jsn", "list/func_xszgp101_1.jsn",
        "list/func_nscg101_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
}

// financial-insights-live
void validate_financial_insights(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-financial-insights-native-v1"),
                  "tdx-market-financial-insights-native-v1",
                  value_or_null(member(document, "schema")));
    const std::vector<std::string> families{
        "buffett-quality", "high-bonus-potential", "investment-property",
        "low-price-sales", "dividend-shortfall", "equity-investment",
        "cash-above-market-cap", "high-receivables", "profit-warning",
        "cash-flow-quality", "earnings-reversal", "steady-growth",
        "quality-growth", "profit-breakout", "dividend-plan"};
    double sum = 0.0; bool population = true;
    for (const auto& kind : families) {
        const auto* value = member_path(document, {"summary", kind});
        population = population && value && value->is_number() &&
            value->as_number() > 0.0;
        if (value && value->is_number()) sum += value->as_number();
    }
    const auto* matched = member(document, "match_count");
    population = population && matched && matched->is_number() &&
        matched->as_number() >= 2000 && std::abs(sum - matched->as_number()) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "all fifteen screen populations are nonempty and exactly reconcile at 2,000+ rows",
                  population);
    bool bonus_unit=false, property_formula=false, dividend_units=false;
    bool investment_unit=false, cash_units=false, receivable_ratio=false;
    bool profit_warning_formula=false, cash_flow_criteria=false;
    bool earnings_reversal_formula=false;
    bool steady_growth_formula=false, quality_growth_criteria=false;
    bool profit_breakout_formula=false, dividend_plan_units=false;
    bool raw_audit=true;
    const auto* records=member(document,"records");
    if(records&&records->is_array())for(const auto& row:records->as_array()){
        const auto* raw=member(row,"raw");raw_audit=raw_audit&&raw&&raw->is_object()&&
            member(row,"source_resource")&&member(row,"source_resource")->is_string();
        if(!raw||!raw->is_object())continue;
        const auto kind=member(row,"kind");
        if(string_is(kind,"high-bonus-potential")){
            const auto source=numeric_value(member(*raw,"ZX1"));const auto normalized=numeric_value(member(row,"latest_share_capital_shares"));
            bonus_unit=bonus_unit||(source&&normalized&&std::abs(*normalized-*source*10000.0)<0.01);
        }else if(string_is(kind,"investment-property")){
            const auto current=numeric_value(member(*raw,"cyje1")),prior=numeric_value(member(*raw,"cyje2")),qoq=numeric_value(member(row,"investment_property_qoq_pct"));
            property_formula=property_formula||(current&&prior&&qoq&&*prior!=0.0&&std::abs(*qoq-(*current-*prior)*100.0/ *prior)<0.000001);
        }else if(string_is(kind,"dividend-shortfall")){
            const auto source=numeric_value(member(*raw,"N001")),normalized=numeric_value(member(row,"latest_dividend_yuan"));
            dividend_units=dividend_units||(source&&normalized&&std::abs(*normalized-*source*10000.0)<0.01);
        }else if(string_is(kind,"equity-investment")){
            const auto source=numeric_value(member(*raw,"ZJE")),normalized=numeric_value(member(row,"investment_total_yuan"));
            investment_unit=investment_unit||(source&&normalized&&std::abs(*normalized-*source*10000.0)<0.01);
        }else if(string_is(kind,"cash-above-market-cap")){
            const auto debt_source=numeric_value(member(*raw,"zcfzl")),debt=numeric_value(member(row,"debt_ratio_pct"));
            const auto dividend_source=numeric_value(member(*raw,"ljfh")),dividend=numeric_value(member(row,"cumulative_dividend_yuan"));
            cash_units=cash_units||(debt_source&&debt&&dividend_source&&dividend&&
                std::abs(*debt-*debt_source*100.0)<0.000001&&std::abs(*dividend-*dividend_source*100000000.0)<0.01);
        }else if(string_is(kind,"high-receivables")){
            const auto source=numeric_value(member(*raw,"zb")),normalized=numeric_value(member(row,"receivables_to_market_cap_pct"));
            receivable_ratio=receivable_ratio||(source&&normalized&&std::abs(*source-*normalized)<0.000001);
        }else if(string_is(kind,"profit-warning")){
            const auto lower=numeric_value(member(*raw,"N007")),upper=numeric_value(member(*raw,"N008"));
            const auto midpoint=numeric_value(member(row,"forecast_profit_midpoint_yuan"));
            const auto actual_source=numeric_value(member(*raw,"N005"));
            const auto actual=numeric_value(member(row,"actual_profit_yuan"));
            profit_warning_formula=profit_warning_formula||(lower&&upper&&midpoint&&actual_source&&actual&&
                std::abs(*midpoint-(*lower+*upper)/2.0)<0.01&&std::abs(*actual-*actual_source)<0.01);
        }else if(string_is(kind,"cash-flow-quality")){
            const auto per_share=numeric_value(member(*raw,"xjl"));
            const auto short_debt=numeric_value(member(*raw,"jxjbl"));
            const auto net_profit=numeric_value(member(*raw,"jlrbl"));
            const auto inventory=numeric_value(member(*raw,"chzcbl"));
            const auto* complete=member(row,"screen_criteria_complete");
            cash_flow_criteria=cash_flow_criteria||(per_share&&short_debt&&net_profit&&inventory&&
                *per_share>0.0&&*short_debt>50.0&&*net_profit>100.0&&*inventory<50.0&&
                complete&&complete->is_bool()&&complete->as_bool());
        }else if(string_is(kind,"earnings-reversal")){
            const auto prior=numeric_value(member(*raw,"JLR1")),next=numeric_value(member(*raw,"JLR2"));
            const auto reported=numeric_value(member(row,"profit_growth_pct"));
            const auto recalculated=numeric_value(member(row,"profit_growth_recalculated_pct"));
            const auto* reversal=member(row,"profit_reversal");
            earnings_reversal_formula=earnings_reversal_formula||(prior&&next&&reported&&recalculated&&*prior!=0.0&&
                std::abs(*recalculated-(*next-*prior)*100.0/std::abs(*prior))<0.000001&&
                std::abs(*reported-*recalculated)<0.011&&*prior<=0.0&&*next>0.0&&
                reversal&&reversal->is_bool()&&reversal->as_bool());
        }else if(string_is(kind,"steady-growth")){
            const auto source=numeric_value(member(*raw,"ljfh"));
            const auto dividend=numeric_value(member(row,"cumulative_dividend_yuan"));
            const auto profit=numeric_value(member(*raw,"zjlr"));
            const auto payout=numeric_value(member(row,"dividend_payout_pct"));
            steady_growth_formula=steady_growth_formula||(source&&dividend&&profit&&payout&&*profit!=0.0&&
                std::abs(*dividend-*source*100000000.0)<0.01&&
                std::abs(*payout-*source*100000000.0*100.0/ *profit)<0.000001);
        }else if(string_is(kind,"quality-growth")){
            const auto prior=numeric_value(member(*raw,"denyysr"));
            const auto current=numeric_value(member(*raw,"dynyysr"));
            const auto growth=numeric_value(member(row,"revenue_growth_t_pct"));
            const auto* complete=member(row,"screen_criteria_complete");
            quality_growth_criteria=quality_growth_criteria||(prior&&current&&growth&&*prior!=0.0&&
                std::abs(*growth-(*current-*prior)*100.0/ *prior)<0.000001&&
                complete&&complete->is_bool()&&complete->as_bool());
        }else if(string_is(kind,"profit-breakout")){
            const auto current=numeric_value(member(*raw,"bqlr"));
            const auto comparison=numeric_value(member(*raw,"sqlr"));
            const auto growth=numeric_value(member(row,"profit_breakout_growth_pct"));
            profit_breakout_formula=profit_breakout_formula||(current&&comparison&&growth&&*comparison!=0.0&&
                std::abs(*growth-(*current-*comparison)*100.0/ *comparison)<0.000001);
        }else if(string_is(kind,"dividend-plan")){
            const auto cash10=numeric_value(member(*raw,"xjfh"));
            const auto cash1=numeric_value(member(row,"cash_dividend_per_share_yuan"));
            const auto shares10=numeric_value(member(*raw,"szbl"));
            const auto shares1=numeric_value(member(row,"stock_transfer_per_share"));
            dividend_plan_units=dividend_plan_units||(cash10&&cash1&&shares10&&shares1&&
                std::abs(*cash1-*cash10/10.0)<0.000001&&
                std::abs(*shares1-*shares10/10.0)<0.000001);
        }
    }else raw_audit=false;
    add_assertion(result,"cfg_units_and_formulas",bonus_unit&&property_formula&&dividend_units&&investment_unit&&cash_units&&receivable_ratio&&profit_warning_formula&&cash_flow_criteria&&earnings_reversal_formula&&steady_growth_formula&&quality_growth_criteria&&profit_breakout_formula&&dividend_plan_units,
                  "share, amount, ratio and derived CFG fields retain calibrated units",
                  bonus_unit&&property_formula&&dividend_units&&investment_unit&&cash_units&&receivable_ratio&&profit_warning_formula&&cash_flow_criteria&&earnings_reversal_formula&&steady_growth_formula&&quality_growth_criteria&&profit_breakout_formula&&dividend_plan_units);
    add_assertion(result,"raw_audit",raw_audit,"all records retain source resource and raw fields",raw_audit);
    const std::vector<std::string> required_sources{
        "list/func_cbpl101_8.jsn","list/func_cbpl106_1.jsn","list/func_cbpl107_1.jsn","list/func_dpsgp_1.jsn",
        "list/func_fhbdb101.jsn","list/func_gqtz101_1.jsn","list/func_gxjcb101.jsn","list/func_gyszk101_1.jsn",
        "list/func_knzx101_1.jsn","list/func_xjl101_1.jsn","list/func_yjfz101_1.jsn",
        "list/func_wjcg101_1.jsn","list/func_lxsnzz101_1.jsn","list/func_tqwclr101.jsn",
        "list/func_qxfa101_1.jsn"};
    const auto* sources=member(document,"sources");bool exact_sources=sources&&sources->is_array()&&sources->size()==required_sources.size();
    for(const auto& source:required_sources)exact_sources=exact_sources&&source_exists(document,source);
    add_assertion(result,"exact_sources",exact_sources,static_cast<std::uint64_t>(required_sources.size()),exact_sources);
}

// gdr-live
void validate_gdr(const Json& document, Json& result) {
    add_assertion(result,"schema",string_is(member(document,"schema"),"tdx-market-gdr-native-v1"),
                  "tdx-market-gdr-native-v1",value_or_null(member(document,"schema")));
    const auto* matched=member(document,"match_count");const auto* underlyings=member_path(document,{"summary","unique_underlyings"});
    const bool population=matched&&matched->is_number()&&matched->as_number()>=20&&underlyings&&underlyings->is_number()&&underlyings->as_number()>=20;
    add_assertion(result,"population",population,"at least twenty GDR-underlying mappings",population);
    bool direct_snapshot=false,raw_audit=true;const auto* records=member(document,"records");
    if(records&&records->is_array())for(const auto& row:records->as_array()){
        const auto* raw=member(row,"raw");raw_audit=raw_audit&&raw&&raw->is_object()&&
            string_is(member(row,"source_resource"),"list/func_gdr101.jsn")&&
            member_path(row,{"underlying","security_id"});
        if(!raw||!raw->is_object())continue;
        const auto source=numeric_value(member(*raw,"yjl"));
        const auto normalized=numeric_value(member(row,"premium_pct"));
        direct_snapshot=direct_snapshot||(source&&normalized&&std::abs(*source-*normalized)<0.000001);
    }else raw_audit=false;
    add_assertion(result,"source_snapshot",direct_snapshot,"premium remains the source quote-date snapshot",direct_snapshot);
    add_assertion(result,"raw_and_underlying",raw_audit,"each row retains raw source and an underlying security",raw_audit);
    const auto* sources=member(document,"sources");const bool exact_sources=sources&&sources->is_array()&&sources->size()==1&&source_exists(document,"list/func_gdr101.jsn");
    add_assertion(result,"exact_sources",exact_sources,1,exact_sources);
}

// fund-calendar-live
void validate_fund_calendar(const Json& document, Json& result) {
    add_assertion(result,"schema",string_is(member(document,"schema"),"tdx-market-fund-calendar-native-v1"),
                  "tdx-market-fund-calendar-native-v1",value_or_null(member(document,"schema")));
    const auto* matched=member(document,"match_count");const auto* funds=member_path(document,{"summary","unique_funds"});
    const auto* types=member_path(document,{"summary","event_type_count"});const auto* categories=member_path(document,{"summary","category_count"});
    const bool population=matched&&matched->is_number()&&matched->as_number()>=250&&funds&&funds->is_number()&&funds->as_number()>=200&&
        types&&types->is_number()&&types->as_number()>=5&&categories&&categories->is_number()&&categories->as_number()>=4;
    add_assertion(result,"population",population,"fund events, funds, types and categories meet minimum populations",population);
    bool content=false,fund_market=false,raw_audit=true;const auto* records=member(document,"records");
    if(records&&records->is_array())for(const auto& row:records->as_array()){
        const auto* raw=member(row,"raw");raw_audit=raw_audit&&raw&&raw->is_object()&&
            string_is(member(row,"source_resource"),"list/func_jjrl301_1.jsn");
        const auto* event_type=member(row,"event_type");const auto* event_date=member(row,"event_date");const auto* event_content=member(row,"content");
        content=content||(event_type&&event_type->is_string()&&!event_type->as_string().empty()&&
            event_date&&event_date->is_string()&&!event_date->as_string().empty()&&
            event_content&&event_content->is_string()&&!event_content->as_string().empty());
        fund_market=fund_market||string_is(member_path(row,{"security","market"}),"fund");
    }else raw_audit=false;
    add_assertion(result,"typed_events",content&&fund_market,"events retain content and market 33 maps to fund",content&&fund_market);
    add_assertion(result,"raw_audit",raw_audit,"all records retain source resource and raw fields",raw_audit);
    const auto* sources=member(document,"sources");const bool exact_sources=sources&&sources->is_array()&&sources->size()==1&&source_exists(document,"list/func_jjrl301_1.jsn");
    add_assertion(result,"exact_sources",exact_sources,1,exact_sources);
}

// recent-watch-live
void validate_recent_watch(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-recent-watch-native-v1"),
                  "tdx-market-recent-watch-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* divergence = member_path(document, {"summary", "earnings-divergence"});
    const auto* foreign = member_path(document, {"summary", "foreign-business"});
    const auto* turnaround = member_path(document, {"summary", "st-turnaround"});
    const auto* matched = member(document, "match_count");
    const bool population = divergence && divergence->is_number() && divergence->as_number() >= 100 &&
        foreign && foreign->is_number() && foreign->as_number() >= 1400 &&
        turnaround && turnaround->is_number() && turnaround->as_number() >= 10 &&
        matched && matched->is_number() && std::abs(divergence->as_number() +
            foreign->as_number() + turnaround->as_number() - matched->as_number()) < 0.5;
    add_assertion(result, "population_reconciliation", population,
                  "three watchlists meet minimum populations and reconcile", population);
    bool foreign_unit = false, snapshot_pct = false, profit_unit = false, raw_audit = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) for (const auto& row : records->as_array()) {
        const auto* raw = member(row, "raw");
        raw_audit = raw_audit && raw && raw->is_object() && member(row, "source_resource");
        if (!raw || !raw->is_object()) continue;
        if (string_is(member(row, "kind"), "foreign-business")) {
            const auto source = numeric_value(member(*raw, "srje"));
            const auto normalized = numeric_value(member(row, "foreign_revenue_yuan"));
            foreign_unit = foreign_unit || (source && normalized &&
                std::abs(*normalized - *source * 100000000.0) < 0.01);
        } else if (string_is(member(row, "kind"), "earnings-divergence")) {
            const auto source = numeric_value(member(*raw, "zf_gj"));
            const auto normalized = numeric_value(member(row, "return_since_announcement_pct"));
            snapshot_pct = snapshot_pct || (source && normalized &&
                std::abs(*normalized - *source) < 0.000001);
        } else if (string_is(member(row, "kind"), "st-turnaround")) {
            const auto source = numeric_value(member(*raw, "ygjl1"));
            const auto normalized = numeric_value(member(row, "profit_lower_yuan"));
            profit_unit = profit_unit || (source && normalized &&
                std::abs(*normalized - *source) < 0.01);
        }
    } else raw_audit = false;
    add_assertion(result, "cfg_units_and_snapshot", foreign_unit && snapshot_pct && profit_unit,
                  "hundred-million foreign amounts, snapshot return and forecast profit units match CFG",
                  foreign_unit && snapshot_pct && profit_unit);
    add_assertion(result, "raw_audit", raw_audit,
                  "all records retain source resource and raw fields", raw_audit);
    const std::vector<std::string> required_sources{
        "list/func_jqgz101_1.jsn", "list/func_jqgz104_1.jsn",
        "list/func_jqgz111_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() && sources->size() == 3;
    for (const auto& source : required_sources) exact_sources = exact_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", exact_sources, static_cast<std::uint64_t>(3), exact_sources);
}

struct InsightContract {
    std::string_view id;
    MarketCorporateCatalogContractValidator validate;
};

constexpr std::array<InsightContract, 11> insight_contracts{{
    {"futures-calendar-live", validate_futures_calendar},
    {"institution-social-security-summary-live", validate_institution_social_security_summary},
    {"institution-development-bank-holdings-live", validate_institution_development_bank_holdings},
    {"patent-statistics-live", validate_patent_statistics},
    {"overview-factors-live", validate_overview_factors},
    {"notable-investor-holdings-live", validate_notable_investor_holdings},
    {"shareholder-signals-live", validate_shareholder_signals},
    {"financial-insights-live", validate_financial_insights},
    {"gdr-live", validate_gdr},
    {"fund-calendar-live", validate_fund_calendar},
    {"recent-watch-live", validate_recent_watch},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < insight_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < insight_contracts.size(); ++right)
            if (insight_contracts[left].id == insight_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_corporate_insights_contract(
    const std::string& contract_id, const Json& document,
    const Json&, Json& result) {
    for (const auto& contract : insight_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
