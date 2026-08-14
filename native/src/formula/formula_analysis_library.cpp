#include "formula_analysis_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tdx {

using namespace formula_engine_detail;
using namespace formula_engine_support;

namespace {

Json parameter_defaults(const Json& formula) {
    Json result = Json::object();
    const auto* parameters = optional(formula, "parameters");
    if (!parameters || !parameters->is_array()) return result;
    for (const auto& parameter : parameters->as_array()) {
        const auto* name = optional(parameter, "name");
        const auto* value = optional(parameter, "default");
        if (name && name->is_string() && value && value->is_number())
            result[upper_ascii(name->as_string())] = value->as_number();
    }
    return result;
}

}  // namespace

Json make_formula_source_definition(
    const std::string& source, const std::string& code, const std::string& kind,
    const std::map<std::string, double>& parameters) {
    const auto actual_code = trim(code);
    if (actual_code.empty()) throw Error("custom formula code is required");
    if (kind != "technical" && kind != "selection" && kind != "expert" &&
        kind != "color-k")
        throw Error("custom formula kind must be technical, selection, expert, or color-k");
    std::vector<std::string> parameter_names;
    Json specs = Json::array();
    for (const auto& [name, value] : parameters) {
        parameter_names.push_back(name);
        Json spec = Json::object();
        spec["name"] = upper_ascii(name);
        spec["default"] = value;
        specs.push_back(std::move(spec));
    }
    Json formula = Json::object();
    formula["code"] = actual_code;
    formula["name"] = actual_code;
    formula["kind_key"] = kind;
    formula["kind_name"] = kind == "technical" ? "技术指标" :
        kind == "selection" ? "条件选股" :
        kind == "expert" ? "专家系统" : "五彩K线";
    formula["category_name"] = "用户源码";
    formula["source_text_available"] = true;
    formula["source_text"] = source;
    formula["parameters"] = std::move(specs);
    formula["analysis"] = analyze_formula_source(source, parameter_names);
    formula["analysis"]["parameter_defaults"] = parameter_defaults(formula);
    formula["outputs"] = formula.at("analysis").at("outputs");
    return formula;
}

Json analyze_formula_library_document(Json library) {
    auto& formulas = library["formulas"].as_array();
    Json by_kind = Json::object();
    std::map<std::string, std::map<std::string, std::uint64_t>> counts;
    std::uint64_t source = 0, syntax = 0, executable = 0, executable_context = 0,
                  explicit_context = 0,
                  read_only_future = 0,
                  external = 0, future = 0, graphics = 0,
                  numeric_signal_safe = 0, presentation_faithful = 0,
                  render_ir_available = 0, semantic_surrogate = 0,
                  render_semantics_materialized = 0,
                  presentation_return_surrogate = 0,
                  unsupported_presentation_directive = 0,
                  degraded_numeric_output = 0;
    for (auto& formula : formulas) {
        const auto* text = optional(formula, "source_text");
        auto analysis = analyze_formula_source(text && text->is_string() ? text->as_string() : "",
                                               formula_parameter_names(formula));
        analysis["parameter_defaults"] = parameter_defaults(formula);
        formula["analysis"] = analysis;
        const auto kind = formula.at("kind_key").as_string();
        auto& row = counts[kind]; ++row["total"];
        if (analysis.at("source_available").as_bool()) { ++source; ++row["source_available"]; }
        if (analysis.at("syntax_supported").as_bool()) { ++syntax; ++row["syntax_supported"]; }
        if (analysis.at("executable").as_bool()) { ++executable; ++row["executable"]; }
        if (const auto* value = optional(analysis, "executable_with_context"); value && value->as_bool()) {
            ++executable_context; ++row["executable_with_context"];
        }
        if (const auto* value = optional(analysis, "explicit_context_bindable");
            value && value->as_bool()) {
            ++explicit_context; ++row["explicit_context_bindable"];
        }
        if (const auto* value = optional(analysis, "read_only_future_executable");
            value && value->as_bool()) {
            ++read_only_future; ++row["read_only_future_executable"];
        }
        const auto* ext = optional(analysis, "has_external_dependency");
        const auto* fut = optional(analysis, "has_future_function");
        const auto* gfx = optional(analysis, "has_graphics");
        const auto* safe = optional(analysis, "numeric_signal_safe");
        const auto* faithful = optional(analysis, "presentation_semantics_faithful");
        const auto* render_ir = optional(analysis, "render_ir_available");
        const auto* surrogate = optional(analysis, "has_semantic_surrogate");
        const auto* render_materialized = optional(
            analysis, "render_semantics_materialized");
        const auto* return_surrogate = optional(
            analysis, "has_presentation_return_surrogate");
        const auto* unsupported_directives = optional(
            analysis, "unsupported_presentation_directives");
        const auto* degraded = optional(analysis, "has_degraded_numeric_output");
        if (ext && ext->as_bool()) { ++external; ++row["external_dependency"]; }
        if (fut && fut->as_bool()) { ++future; ++row["future_function"]; }
        if (gfx && gfx->as_bool()) { ++graphics; ++row["graphics"]; }
        if (safe && safe->as_bool()) { ++numeric_signal_safe; ++row["numeric_signal_safe"]; }
        if (faithful && faithful->as_bool()) {
            ++presentation_faithful; ++row["presentation_semantics_faithful"];
        }
        if (render_ir && render_ir->as_bool()) {
            ++render_ir_available; ++row["render_ir_available"];
        }
        if (surrogate && surrogate->as_bool()) {
            ++semantic_surrogate; ++row["semantic_surrogate"];
        }
        if (render_materialized && render_materialized->as_bool()) {
            ++render_semantics_materialized;
            ++row["render_semantics_materialized"];
        }
        if (return_surrogate && return_surrogate->as_bool()) {
            ++presentation_return_surrogate;
            ++row["presentation_return_surrogate"];
        }
        if (unsupported_directives && unsupported_directives->is_array() &&
            unsupported_directives->size() > 0) {
            ++unsupported_presentation_directive;
            ++row["unsupported_presentation_directive"];
        }
        if (degraded && degraded->as_bool()) {
            ++degraded_numeric_output; ++row["degraded_numeric_output"];
        }
    }
    for (const auto& [kind, values] : counts) {
        Json row = Json::object(); for (const auto& [name, value] : values) row[name] = value;
        by_kind[kind] = std::move(row);
    }
    Json coverage = Json::object(); coverage["total"] = static_cast<std::uint64_t>(formulas.size());
    coverage["source_available"] = source; coverage["syntax_supported"] = syntax;
    coverage["executable"] = executable; coverage["executable_with_context"] = executable_context;
    coverage["explicit_context_bindable"] = explicit_context;
    coverage["read_only_future_executable"] = read_only_future;
    coverage["external_dependency"] = external;
    coverage["future_function"] = future; coverage["graphics"] = graphics;
    coverage["numeric_signal_safe"] = numeric_signal_safe;
    coverage["presentation_semantics_faithful"] = presentation_faithful;
    coverage["render_ir_available"] = render_ir_available;
    coverage["semantic_surrogate"] = semantic_surrogate;
    coverage["render_semantics_materialized"] = render_semantics_materialized;
    coverage["presentation_return_surrogate"] = presentation_return_surrogate;
    coverage["unsupported_presentation_directive"] =
        unsupported_presentation_directive;
    coverage["degraded_numeric_output"] = degraded_numeric_output;
    Json capabilities = Json::object();
    capabilities["schema"] = "tdx-formula-interpreter-capabilities-v1";
    capabilities["formula_engine"] = "tdx-source-interpreter-v1";
    capabilities["supported_function_count"] =
        static_cast<std::uint64_t>(supported_functions.size());
    capabilities["supported_functions"] = strings_json(supported_functions);
    std::set<std::string> automatic_symbols = builtin_symbols;
    automatic_symbols.insert(market_symbols.begin(), market_symbols.end());
    automatic_symbols.insert(constants.begin(), constants.end());
    automatic_symbols.insert(string_symbols.begin(), string_symbols.end());
    automatic_symbols.insert(intrinsic_formula_symbols.begin(),
                             intrinsic_formula_symbols.end());
    automatic_symbols.insert(custom_formula_directional_bar_functions.begin(),
                             custom_formula_directional_bar_functions.end());
    automatic_symbols.insert(custom_formula_adjustment_functions.begin(),
                             custom_formula_adjustment_functions.end());
    automatic_symbols.insert(custom_formula_security_stat_functions.begin(),
                             custom_formula_security_stat_functions.end());
    automatic_symbols.insert(custom_formula_industry_valuation_functions.begin(),
                             custom_formula_industry_valuation_functions.end());
    automatic_symbols.insert(custom_formula_market_breadth_functions.begin(),
                             custom_formula_market_breadth_functions.end());
    automatic_symbols.insert(custom_formula_dynamic_quote_functions.begin(),
                             custom_formula_dynamic_quote_functions.end());
    automatic_symbols.insert(custom_formula_security_relation_functions.begin(),
                             custom_formula_security_relation_functions.end());
    capabilities["automatic_symbol_count"] =
        static_cast<std::uint64_t>(automatic_symbols.size());
    capabilities["automatic_symbols"] = strings_json(automatic_symbols);
    capabilities["explicit_context_symbols"] = strings_json(explicit_context_symbols);
    capabilities["custom_formula_core_function_count"] =
        static_cast<std::uint64_t>(custom_formula_core_functions.size());
    capabilities["custom_formula_core_functions"] =
        strings_json(custom_formula_core_functions);
    capabilities["custom_formula_core_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_core_symbols.size());
    capabilities["custom_formula_core_symbols"] =
        strings_json(custom_formula_core_symbols);
    capabilities["custom_formula_transform_function_count"] =
        static_cast<std::uint64_t>(custom_formula_transform_functions.size());
    capabilities["custom_formula_transform_functions"] =
        strings_json(custom_formula_transform_functions);
    capabilities["custom_formula_future_path_function_count"] =
        static_cast<std::uint64_t>(custom_formula_future_path_functions.size());
    capabilities["custom_formula_future_path_functions"] =
        strings_json(custom_formula_future_path_functions);
    capabilities["custom_formula_random_function_count"] =
        static_cast<std::uint64_t>(custom_formula_random_functions.size());
    capabilities["custom_formula_random_functions"] =
        strings_json(custom_formula_random_functions);
    capabilities["custom_formula_directional_bar_function_count"] =
        static_cast<std::uint64_t>(custom_formula_directional_bar_functions.size());
    capabilities["custom_formula_directional_bar_functions"] =
        strings_json(custom_formula_directional_bar_functions);
    capabilities["custom_formula_directional_bar_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_directional_bar_functions.size());
    capabilities["custom_formula_directional_bar_symbols"] =
        strings_json(custom_formula_directional_bar_functions);
    capabilities["custom_formula_adjustment_function_count"] =
        static_cast<std::uint64_t>(custom_formula_adjustment_functions.size());
    capabilities["custom_formula_adjustment_functions"] =
        strings_json(custom_formula_adjustment_functions);
    capabilities["custom_formula_adjustment_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_adjustment_functions.size());
    capabilities["custom_formula_adjustment_symbols"] =
        strings_json(custom_formula_adjustment_functions);
    capabilities["custom_formula_host_calendar_function_count"] =
        static_cast<std::uint64_t>(custom_formula_host_calendar_functions.size());
    capabilities["custom_formula_host_calendar_functions"] =
        strings_json(custom_formula_host_calendar_functions);
    capabilities["custom_formula_capital_turnover_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_capital_turnover_functions.size());
    capabilities["custom_formula_capital_turnover_functions"] =
        strings_json(custom_formula_capital_turnover_functions);
    capabilities["custom_formula_machine_clock_function_count"] =
        static_cast<std::uint64_t>(custom_formula_machine_clock_functions.size());
    capabilities["custom_formula_machine_clock_functions"] =
        strings_json(custom_formula_machine_clock_functions);
    capabilities["custom_formula_sequence_statistics_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_sequence_statistics_functions.size());
    capabilities["custom_formula_sequence_statistics_functions"] =
        strings_json(custom_formula_sequence_statistics_functions);
    capabilities["custom_formula_rolling_variance_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_rolling_variance_functions.size());
    capabilities["custom_formula_rolling_variance_functions"] =
        strings_json(custom_formula_rolling_variance_functions);
    capabilities["custom_formula_benchmark_cumulative_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_benchmark_cumulative_functions.size());
    capabilities["custom_formula_benchmark_cumulative_functions"] =
        strings_json(custom_formula_benchmark_cumulative_functions);
    capabilities["custom_formula_calendar_filter_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_calendar_filter_functions.size());
    capabilities["custom_formula_calendar_filter_functions"] =
        strings_json(custom_formula_calendar_filter_functions);
    capabilities["custom_formula_calendar_filter_symbol_count"] =
        static_cast<std::uint64_t>(
            custom_formula_calendar_filter_symbols.size());
    capabilities["custom_formula_calendar_filter_symbols"] =
        strings_json(custom_formula_calendar_filter_symbols);
    capabilities["custom_formula_security_string_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_security_string_functions.size());
    capabilities["custom_formula_security_string_functions"] =
        strings_json(custom_formula_security_string_functions);
    capabilities["custom_formula_security_string_symbol_count"] =
        static_cast<std::uint64_t>(
            custom_formula_security_string_symbols.size());
    capabilities["custom_formula_security_string_symbols"] =
        strings_json(custom_formula_security_string_symbols);
    capabilities["custom_formula_contract_metadata_symbol_count"] =
        static_cast<std::uint64_t>(
            custom_formula_contract_metadata_symbols.size());
    capabilities["custom_formula_contract_metadata_symbols"] =
        strings_json(custom_formula_contract_metadata_symbols);
    capabilities["custom_formula_security_stat_function_count"] =
        static_cast<std::uint64_t>(custom_formula_security_stat_functions.size());
    capabilities["custom_formula_security_stat_functions"] =
        strings_json(custom_formula_security_stat_functions);
    capabilities["custom_formula_security_stat_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_security_stat_functions.size());
    capabilities["custom_formula_security_stat_symbols"] =
        strings_json(custom_formula_security_stat_functions);
    capabilities["custom_formula_industry_valuation_function_count"] =
        static_cast<std::uint64_t>(custom_formula_industry_valuation_functions.size());
    capabilities["custom_formula_industry_valuation_functions"] =
        strings_json(custom_formula_industry_valuation_functions);
    capabilities["custom_formula_industry_valuation_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_industry_valuation_functions.size());
    capabilities["custom_formula_industry_valuation_symbols"] =
        strings_json(custom_formula_industry_valuation_functions);
    capabilities["custom_formula_market_breadth_function_count"] =
        static_cast<std::uint64_t>(custom_formula_market_breadth_functions.size());
    capabilities["custom_formula_market_breadth_functions"] =
        strings_json(custom_formula_market_breadth_functions);
    capabilities["custom_formula_market_breadth_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_market_breadth_functions.size());
    capabilities["custom_formula_market_breadth_symbols"] =
        strings_json(custom_formula_market_breadth_functions);
    capabilities["custom_formula_dynamic_quote_function_count"] =
        static_cast<std::uint64_t>(custom_formula_dynamic_quote_functions.size());
    capabilities["custom_formula_dynamic_quote_functions"] =
        strings_json(custom_formula_dynamic_quote_functions);
    capabilities["custom_formula_dynamic_quote_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_dynamic_quote_functions.size());
    capabilities["custom_formula_dynamic_quote_symbols"] =
        strings_json(custom_formula_dynamic_quote_functions);
    capabilities["custom_formula_security_relation_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_security_relation_functions.size());
    capabilities["custom_formula_security_relation_functions"] =
        strings_json(custom_formula_security_relation_functions);
    capabilities["custom_formula_security_relation_symbol_count"] =
        static_cast<std::uint64_t>(
            custom_formula_security_relation_functions.size());
    capabilities["custom_formula_security_relation_symbols"] =
        strings_json(custom_formula_security_relation_functions);
    capabilities["custom_formula_security_relation_text_symbol_count"] =
        static_cast<std::uint64_t>(
            custom_formula_security_relation_text_symbols.size());
    capabilities["custom_formula_security_relation_text_symbols"] =
        strings_json(custom_formula_security_relation_text_symbols);
    capabilities["custom_formula_divfactor_function_count"] =
        static_cast<std::uint64_t>(custom_formula_divfactor_functions.size());
    capabilities["custom_formula_divfactor_functions"] =
        strings_json(custom_formula_divfactor_functions);
    capabilities["custom_formula_kline_auxiliary_symbol_count"] =
        static_cast<std::uint64_t>(
            custom_formula_kline_auxiliary_symbols.size());
    capabilities["custom_formula_kline_auxiliary_symbols"] =
        strings_json(custom_formula_kline_auxiliary_symbols);
    capabilities["custom_formula_block_metadata_function_count"] =
        static_cast<std::uint64_t>(custom_formula_block_metadata_functions.size());
    capabilities["custom_formula_block_metadata_functions"] =
        strings_json(custom_formula_block_metadata_functions);
    capabilities["custom_formula_indicator_aggregate_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_indicator_aggregate_functions.size());
    capabilities["custom_formula_indicator_aggregate_functions"] =
        strings_json(custom_formula_indicator_aggregate_functions);
    capabilities["custom_formula_block_metadata_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_block_metadata_symbols.size());
    capabilities["custom_formula_block_metadata_symbols"] =
        strings_json(custom_formula_block_metadata_symbols);
    capabilities["custom_formula_single_point_function_count"] =
        static_cast<std::uint64_t>(custom_formula_single_point_functions.size());
    capabilities["custom_formula_single_point_functions"] =
        strings_json(custom_formula_single_point_functions);
    capabilities["custom_formula_external_signal_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_external_signal_functions.size());
    capabilities["custom_formula_external_signal_functions"] =
        strings_json(custom_formula_external_signal_functions);
    capabilities["custom_formula_external_series_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_external_series_functions.size());
    capabilities["custom_formula_external_series_functions"] =
        strings_json(custom_formula_external_series_functions);
    capabilities["custom_formula_type167_text_symbol_count"] =
        static_cast<std::uint64_t>(custom_formula_type167_text_symbols.size());
    capabilities["custom_formula_type167_text_symbols"] =
        strings_json(custom_formula_type167_text_symbols);
    capabilities["custom_formula_security_score_function_count"] =
        static_cast<std::uint64_t>(
            custom_formula_security_score_functions.size());
    capabilities["custom_formula_security_score_functions"] =
        strings_json(custom_formula_security_score_functions);
    Json registry_evidence = Json::object();
    registry_evidence["profile"] = "tdx-2025-11-14";
    registry_evidence["source_sha256"] =
        "13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5";
    registry_evidence["static_registry_entry_count"] = 390;
    registry_evidence["static_registry_unique_name_count"] = 390;
    std::set<std::string> boundary_names;
    const auto include_boundary = [&](const std::set<std::string>& names) {
        boundary_names.insert(names.begin(), names.end());
    };
    include_boundary(tcalc_registry_syntax_only_names);
    include_boundary(tcalc_registry_broker_private_signal_names);
    include_boundary(tcalc_registry_level2_order_flow_names);
    include_boundary(tcalc_registry_live_trading_state_names);
    include_boundary(tcalc_registry_plugin_callback_names);
    registry_evidence["syntax_only_name_count"] =
        static_cast<std::uint64_t>(tcalc_registry_syntax_only_names.size());
    registry_evidence["syntax_only_names"] =
        strings_json(tcalc_registry_syntax_only_names);
    registry_evidence["broker_private_signal_name_count"] =
        static_cast<std::uint64_t>(
            tcalc_registry_broker_private_signal_names.size());
    registry_evidence["broker_private_signal_names"] =
        strings_json(tcalc_registry_broker_private_signal_names);
    registry_evidence["level2_order_flow_name_count"] =
        static_cast<std::uint64_t>(tcalc_registry_level2_order_flow_names.size());
    registry_evidence["level2_order_flow_names"] =
        strings_json(tcalc_registry_level2_order_flow_names);
    registry_evidence["level2_context_capable_name_count"] =
        static_cast<std::uint64_t>(tcalc_registry_level2_order_flow_names.size());
    registry_evidence["level2_context_source"] =
        "authorized caller series; TCalc callback type 31 uses 184-byte daily records, ISBUYORDER uses callback type 104";
    registry_evidence["live_trading_state_name_count"] =
        static_cast<std::uint64_t>(
            tcalc_registry_live_trading_state_names.size());
    registry_evidence["live_trading_state_names"] =
        strings_json(tcalc_registry_live_trading_state_names);
    registry_evidence["live_trading_context_capable_name_count"] =
        static_cast<std::uint64_t>(
            tcalc_registry_live_trading_context_names.size());
    registry_evidence["live_trading_context_capable_names"] =
        strings_json(tcalc_registry_live_trading_context_names);
    registry_evidence["plugin_callback_name_count"] =
        static_cast<std::uint64_t>(tcalc_registry_plugin_callback_names.size());
    registry_evidence["plugin_callback_names"] =
        strings_json(tcalc_registry_plugin_callback_names);
    registry_evidence["static_registry_boundary_name_count"] =
        static_cast<std::uint64_t>(boundary_names.size());
    registry_evidence["static_registry_boundary_names"] =
        strings_json(boundary_names);
    registry_evidence["static_registry_recognized_name_count"] =
        static_cast<std::uint64_t>(390 - boundary_names.size());
    registry_evidence["static_registry_fully_classified"] =
        boundary_names.size() == 71;
    registry_evidence["remaining_public_non_l2_candidate_count"] = 0;
    registry_evidence["broker_private_signal_context_policy"] =
        "caller-supplied-exact-series-only;no-public-resolver";
    registry_evidence["local_signal_context_policy"] =
        "SIGNALS_SYS/SIGNALS_USER use exact local TdxW selector-34/36 files; SIGNALS_QS remains caller-supplied";
    registry_evidence["level2_policy"] =
        "authorized-caller-series-only;no-inferred-values";
    registry_evidence["live_trading_policy"] =
        "34 state symbols accept caller-owned explicit scalar/date-time series only; "
        "no account access; ORDERBUY/ORDERSELL/CLOSEALLD/CLOSEALLK remain unavailable actions";
    registry_evidence["plugin_policy"] =
        "external-dll-callbacks-not-loaded";
    registry_evidence["record_size_bytes"] = 71;
    registry_evidence["initializer"] = "TCalc.dll!sub_100C6870";
    registry_evidence["scope"] =
        "offline-static-registry-evidence;not-a-claim-that-all-entries-are-runtime-independent";
    capabilities["tcalc_registry_evidence"] = std::move(registry_evidence);
    coverage["capabilities"] = std::move(capabilities);
    coverage["by_kind"] = std::move(by_kind);
    coverage["analysis_schema_version"] = 7;
    coverage["formula_engine"] = "tdx-source-interpreter-v1";
    library["analysis_schema_version"] = 7;
    library["formula_engine"] = "tdx-source-interpreter-v1";
    library["coverage"] = std::move(coverage);
    return library;
}


}  // namespace tdx
