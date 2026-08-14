#include "recon_contract_formula_foundation_internal.hpp"

#include "recon_contract_internal.hpp"

#include <array>
#include <set>
#include <string>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

struct CapabilitySets {
    std::set<std::string> core_functions;
    std::set<std::string> core_symbols;
    std::set<std::string> sequence_statistics_functions;
    std::set<std::string> rolling_variance_functions;
    std::set<std::string> benchmark_cumulative_functions;
    std::set<std::string> calendar_filter_functions;
    std::set<std::string> calendar_filter_symbols;
    std::set<std::string> security_string_functions;
    std::set<std::string> security_string_symbols;
    std::set<std::string> block_metadata_functions;
    std::set<std::string> block_metadata_symbols;
    std::set<std::string> single_point_functions;
    std::set<std::string> external_signal_functions;
    std::set<std::string> external_series_functions;
    std::set<std::string> type167_text_symbols;
    std::set<std::string> security_stat_functions;
    std::set<std::string> security_stat_symbols;
    std::set<std::string> industry_valuation_functions;
    std::set<std::string> industry_valuation_symbols;
    std::set<std::string> market_breadth_functions;
    std::set<std::string> market_breadth_symbols;
    std::set<std::string> dynamic_quote_functions;
    std::set<std::string> dynamic_quote_symbols;
    std::set<std::string> security_relation_functions;
    std::set<std::string> security_relation_symbols;
    std::set<std::string> security_relation_text_symbols;
    std::set<std::string> divfactor_functions;
    std::set<std::string> transform_functions;
    std::set<std::string> future_path_functions;
    std::set<std::string> random_functions;
    std::set<std::string> security_score_functions;
    std::set<std::string> directional_bar_functions;
    std::set<std::string> directional_bar_symbols;
    std::set<std::string> adjustment_functions;
    std::set<std::string> adjustment_symbols;
    std::set<std::string> host_calendar_functions;
    std::set<std::string> capital_turnover_functions;
    std::set<std::string> machine_clock_functions;
    std::set<std::string> kline_auxiliary_symbols;
};

using CapabilitySetMember = std::set<std::string> CapabilitySets::*;

struct CapabilityBinding {
    std::string_view array_field;
    std::string_view count_field;
    CapabilitySetMember target;
};

constexpr std::array<CapabilityBinding, 39> kCapabilityBindings{{
    {"custom_formula_core_functions", "custom_formula_core_function_count", &CapabilitySets::core_functions},
    {"custom_formula_core_symbols", "custom_formula_core_symbol_count", &CapabilitySets::core_symbols},
    {"custom_formula_sequence_statistics_functions", "custom_formula_sequence_statistics_function_count", &CapabilitySets::sequence_statistics_functions},
    {"custom_formula_rolling_variance_functions", "custom_formula_rolling_variance_function_count", &CapabilitySets::rolling_variance_functions},
    {"custom_formula_benchmark_cumulative_functions", "custom_formula_benchmark_cumulative_function_count", &CapabilitySets::benchmark_cumulative_functions},
    {"custom_formula_calendar_filter_functions", "custom_formula_calendar_filter_function_count", &CapabilitySets::calendar_filter_functions},
    {"custom_formula_calendar_filter_symbols", "custom_formula_calendar_filter_symbol_count", &CapabilitySets::calendar_filter_symbols},
    {"custom_formula_security_string_functions", "custom_formula_security_string_function_count", &CapabilitySets::security_string_functions},
    {"custom_formula_security_string_symbols", "custom_formula_security_string_symbol_count", &CapabilitySets::security_string_symbols},
    {"custom_formula_block_metadata_functions", "custom_formula_block_metadata_function_count", &CapabilitySets::block_metadata_functions},
    {"custom_formula_block_metadata_symbols", "custom_formula_block_metadata_symbol_count", &CapabilitySets::block_metadata_symbols},
    {"custom_formula_single_point_functions", "custom_formula_single_point_function_count", &CapabilitySets::single_point_functions},
    {"custom_formula_external_signal_functions", "custom_formula_external_signal_function_count", &CapabilitySets::external_signal_functions},
    {"custom_formula_external_series_functions", "custom_formula_external_series_function_count", &CapabilitySets::external_series_functions},
    {"custom_formula_type167_text_symbols", "custom_formula_type167_text_symbol_count", &CapabilitySets::type167_text_symbols},
    {"custom_formula_security_stat_functions", "custom_formula_security_stat_function_count", &CapabilitySets::security_stat_functions},
    {"custom_formula_security_stat_symbols", "custom_formula_security_stat_symbol_count", &CapabilitySets::security_stat_symbols},
    {"custom_formula_industry_valuation_functions", "custom_formula_industry_valuation_function_count", &CapabilitySets::industry_valuation_functions},
    {"custom_formula_industry_valuation_symbols", "custom_formula_industry_valuation_symbol_count", &CapabilitySets::industry_valuation_symbols},
    {"custom_formula_market_breadth_functions", "custom_formula_market_breadth_function_count", &CapabilitySets::market_breadth_functions},
    {"custom_formula_market_breadth_symbols", "custom_formula_market_breadth_symbol_count", &CapabilitySets::market_breadth_symbols},
    {"custom_formula_dynamic_quote_functions", "custom_formula_dynamic_quote_function_count", &CapabilitySets::dynamic_quote_functions},
    {"custom_formula_dynamic_quote_symbols", "custom_formula_dynamic_quote_symbol_count", &CapabilitySets::dynamic_quote_symbols},
    {"custom_formula_security_relation_functions", "custom_formula_security_relation_function_count", &CapabilitySets::security_relation_functions},
    {"custom_formula_security_relation_symbols", "custom_formula_security_relation_symbol_count", &CapabilitySets::security_relation_symbols},
    {"custom_formula_security_relation_text_symbols", "custom_formula_security_relation_text_symbol_count", &CapabilitySets::security_relation_text_symbols},
    {"custom_formula_divfactor_functions", "custom_formula_divfactor_function_count", &CapabilitySets::divfactor_functions},
    {"custom_formula_transform_functions", "custom_formula_transform_function_count", &CapabilitySets::transform_functions},
    {"custom_formula_future_path_functions", "custom_formula_future_path_function_count", &CapabilitySets::future_path_functions},
    {"custom_formula_random_functions", "custom_formula_random_function_count", &CapabilitySets::random_functions},
    {"custom_formula_security_score_functions", "custom_formula_security_score_function_count", &CapabilitySets::security_score_functions},
    {"custom_formula_directional_bar_functions", "custom_formula_directional_bar_function_count", &CapabilitySets::directional_bar_functions},
    {"custom_formula_directional_bar_symbols", "custom_formula_directional_bar_symbol_count", &CapabilitySets::directional_bar_symbols},
    {"custom_formula_adjustment_functions", "custom_formula_adjustment_function_count", &CapabilitySets::adjustment_functions},
    {"custom_formula_adjustment_symbols", "custom_formula_adjustment_symbol_count", &CapabilitySets::adjustment_symbols},
    {"custom_formula_host_calendar_functions", "custom_formula_host_calendar_function_count", &CapabilitySets::host_calendar_functions},
    {"custom_formula_capital_turnover_functions", "custom_formula_capital_turnover_function_count", &CapabilitySets::capital_turnover_functions},
    {"custom_formula_machine_clock_functions", "custom_formula_machine_clock_function_count", &CapabilitySets::machine_clock_functions},
    {"custom_formula_kline_auxiliary_symbols", "custom_formula_kline_auxiliary_symbol_count", &CapabilitySets::kline_auxiliary_symbols},
}};

struct NumberExpectation {
    std::string_view field;
    double value;
};

constexpr std::array<NumberExpectation, 10> kRegistryCounts{{
    {"static_registry_entry_count", 390.0},
    {"static_registry_unique_name_count", 390.0},
    {"static_registry_boundary_name_count", 71.0},
    {"static_registry_recognized_name_count", 319.0},
    {"syntax_only_name_count", 4.0},
    {"broker_private_signal_name_count", 1.0},
    {"level2_order_flow_name_count", 13.0},
    {"live_trading_state_name_count", 38.0},
    {"plugin_callback_name_count", 15.0},
    {"remaining_public_non_l2_candidate_count", 0.0},
}};

constexpr bool unique_binding_fields() {
    for (std::size_t left = 0; left < kCapabilityBindings.size(); ++left)
        for (std::size_t right = left + 1; right < kCapabilityBindings.size(); ++right)
            if (kCapabilityBindings[left].array_field == kCapabilityBindings[right].array_field ||
                kCapabilityBindings[left].count_field == kCapabilityBindings[right].count_field)
                return false;
    return true;
}

static_assert(unique_binding_fields(), "formula capability bindings must be unique");

CapabilitySets expected_capabilities() {
    CapabilitySets result;
    result.core_functions = {
        "ACOS", "ASIN", "ATAN", "CONST", "CONSTA", "COS", "EXISTR",
        "FRACPART", "RANGE", "ROUND2", "SGN", "SIGN", "SIN", "TAN"};
    result.core_symbols = {
        "BARSTATUS", "DAY", "MONTH", "TOTALBARSCOUNT", "WEEKDAY", "YEAR"};
    result.sequence_statistics_functions = {
        "BARSLASTS", "BARSSINCEN", "BETAEX", "COVAR", "FILTERX",
        "FINDHIGH", "FINDHIGHBARS", "FINDLOW", "FINDLOWBARS", "RELATE",
        "TMA", "XMA"};
    result.rolling_variance_functions = {
        "AMA", "DEVSQ", "HHVLLV", "HOD", "IFF", "IFN", "ISVALID",
        "LOD", "MAX6", "MIN6", "MULAR", "REFV", "STDP", "VAR", "VARP"};
    result.benchmark_cumulative_functions = {"BETA", "SUMBARSX"};
    result.calendar_filter_functions = {
        "ALIGNRIGHT", "DATETOCUR", "DAYTODATE", "SECTOTIME", "TFILT",
        "TFILTER", "TIMETOSEC", "TTFILTER"};
    result.calendar_filter_symbols = {"TIME2", "WEEKOFYEAR"};
    result.security_string_functions = {
        "CODELIKE", "FINDSTR", "NAMEINCLUDE", "NAMELIKE", "NOT",
        "STR2CON", "STRCAT6", "STRLEN", "STRSPACE", "SUBSTR",
        "UPDOWN", "VAR2STR", "VARCAT", "VARCAT6", "IST0CODE",
        "ISSTCODE", "ISQUITCODE", "ISQHQQCODE"};
    result.security_string_symbols = {"STKNAME"};
    result.block_metadata_functions = {
        "BLOCKSETNUM", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE", "HORCALC", "INBLOCK"};
    result.block_metadata_symbols = {
        "FGBLOCK", "FGBLOCKNUM", "GNBLOCKNUM", "HYZSCODE", "SIMIBLOCK",
        "ZDBLOCK", "ZDBLOCKNUM", "ZHBLOCK", "ZHBLOCKNUM", "ZSBLOCK", "ZSBLOCKNUM"};
    result.single_point_functions = {"BKJYONE", "FINONE", "GPJYONE", "GPONEDAT", "SCJYONE"};
    result.external_signal_functions = {"EXTERNSTR", "EXTERNVALUE"};
    result.external_series_functions = {
        "EXTDATA_USER", "SIGNALS_SYS", "SIGNALS_USER"};
    result.type167_text_symbols = {"LEVEL1HYBLOCK", "MAINBUSINESS", "MOREHYBLOCK"};
    result.security_stat_functions = {"BETAVALUE", "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT"};
    result.security_stat_symbols = result.security_stat_functions;
    result.industry_valuation_functions = {"HYSJL", "HYSYL"};
    result.industry_valuation_symbols = result.industry_valuation_functions;
    result.market_breadth_functions = {"INDEXADV", "INDEXDEC"};
    result.market_breadth_symbols = result.market_breadth_functions;
    result.dynamic_quote_functions = {"DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS"};
    result.dynamic_quote_symbols = result.dynamic_quote_functions;
    result.security_relation_functions = {"DPZSCODE", "DPZSNAME", "UNDERCODE", "UNDERLYC"};
    result.security_relation_symbols = result.security_relation_functions;
    result.security_relation_text_symbols = {"DPZSCODE", "DPZSNAME", "UNDERCODE"};
    result.divfactor_functions = {"DIVFACTOR"};
    result.transform_functions = {"FFTRANS", "NEWSAR"};
    result.future_path_functions = {"ZIGA"};
    result.random_functions = {"RAND"};
    result.security_score_functions = {"SAFESCORE", "SHINESCORE"};
    result.directional_bar_functions = {"DCLOSE", "DHIGH", "DLOW", "DOPEN", "DVOL"};
    result.directional_bar_symbols = result.directional_bar_functions;
    result.adjustment_functions = {"TQFLAG"};
    result.adjustment_symbols = result.adjustment_functions;
    result.host_calendar_functions = {"ISJYDATE", "LOCALDAYNUM"};
    result.capital_turnover_functions = {"LFS"};
    result.machine_clock_functions = {"MACHINEDATE", "MACHINETIME", "MACHINEWEEK"};
    result.kline_auxiliary_symbols = {"QHJSJ", "ZSTJJ"};
    return result;
}

CapabilitySets collect_capabilities(const Json& capabilities) {
    CapabilitySets result;
    for (const auto& binding : kCapabilityBindings) {
        const auto* values = member(capabilities, binding.array_field);
        if (!values || !values->is_array()) continue;
        auto& target = result.*(binding.target);
        for (const auto& value : values->as_array())
            if (value.is_string()) target.insert(value.as_string());
    }
    return result;
}

}  // namespace

bool validate_formula_capabilities(const Json& capabilities) {
    const auto* registry = member(capabilities, "tcalc_registry_evidence");
    const auto function_count = numeric_value(member(capabilities, "supported_function_count"));
    const auto automatic_symbol_count = numeric_value(member(capabilities, "automatic_symbol_count"));
    if (!registry || !function_count || !automatic_symbol_count ||
        !string_is(member(capabilities, "schema"),
                   "tdx-formula-interpreter-capabilities-v1") ||
        *function_count < 262.0 || *automatic_symbol_count < 87.0)
        return false;

    const auto actual = collect_capabilities(capabilities);
    const auto expected = expected_capabilities();
    for (const auto& binding : kCapabilityBindings) {
        const auto& expected_values = expected.*(binding.target);
        if (!number_is(member(capabilities, binding.count_field),
                       static_cast<double>(expected_values.size())) ||
            actual.*(binding.target) != expected_values)
            return false;
    }
    for (const auto& expectation : kRegistryCounts)
        if (!number_is(member(*registry, expectation.field), expectation.value))
            return false;
    return bool_is(member(*registry, "static_registry_fully_classified"), true) &&
        string_is(member(*registry, "profile"), "tdx-2025-11-14");
}

}  // namespace tdx::recon_contract_detail
