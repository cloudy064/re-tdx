#include "tdx/formula_engine.hpp"
#include "formula_environment_internal.hpp"
#include "formula_engine_internal.hpp"
#include "formula_engine_support_internal.hpp"
#include "formula_context_hk_finance_internal.hpp"
#include "formula_language_internal.hpp"
#include "formula_render_internal.hpp"
#include "formula_runtime_internal.hpp"
#include "formula_trade_event_filter_internal.hpp"
#include "formula_trade_event_ir_internal.hpp"

#include "tdx/common.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_render_detail;
using namespace formula_runtime_detail;
using namespace formula_trade_event_filter_detail;
using namespace formula_trade_event_ir_detail;

namespace {

bool materialized_context_binding(const Json& context,
                                  std::string_view binding) {
    if (!context.is_object()) return false;
    const auto normalized = upper_ascii(std::string(binding));
    const auto separator = normalized.find('#');
    if (separator != std::string::npos && separator > 0 &&
        separator + 1 < normalized.size()) {
        const auto group = lower_ascii(normalized.substr(0, separator));
        if (group == "finance" || group == "finvalue" ||
            group == "dynainfo") {
            if (const auto* values = optional(context, group);
                values && values->is_object()) {
                const auto found = values->as_object().find(
                    normalized.substr(separator + 1));
                if (found != values->as_object().end() &&
                    found->second.is_number())
                    return true;
            }
        }
    }

    if (const auto* scalars = optional(context, "formula_scalar_bindings");
        scalars && scalars->is_object())
        for (const auto& [name, value] : scalars->as_object())
            if (upper_ascii(name) == normalized &&
                (value.is_number() || value.is_null()))
                return true;
    if (const auto* symbols = optional(context, "symbols");
        symbols && symbols->is_object())
        for (const auto& [name, value] : symbols->as_object())
            if (upper_ascii(name) == normalized && value.is_number())
                return true;
    if (const auto* series = optional(context, "series");
        series && series->is_object())
        for (const auto& [name, value] : series->as_object())
            if (upper_ascii(name) == normalized && value.is_object())
                return true;
    return false;
}

bool context_binding_requires_materialization(std::string_view binding) {
    constexpr std::string_view groups[]{"FINANCE#", "FINVALUE#",
                                        "DYNAINFO#"};
    return std::any_of(std::begin(groups), std::end(groups),
                       [binding](std::string_view group) {
                           return binding.rfind(group, 0) == 0;
                       });
}

std::set<std::string> missing_materialized_context_bindings(
    const Json& analysis, const Json* context) {
    std::set<std::string> result;
    const auto* required = optional(analysis, "context_bindings_required");
    if (!required || !required->is_array()) return result;
    for (const auto& binding : required->as_array()) {
        if (!binding.is_string()) continue;
        const auto normalized = upper_ascii(binding.as_string());
        if (!context_binding_requires_materialization(normalized)) continue;
        if (!context || !materialized_context_binding(*context, normalized))
            result.insert(binding.as_string());
    }
    return result;
}

}  // namespace

Json evaluate_formula_source_document(Json kline_document, const std::string& source,
                                      const std::map<std::string, double>& parameters,
                                      const std::string& formula_code,
                                      const Json* context) {
    auto bars = read_bars(kline_document);
    if (bars.empty()) throw Error("formula execution requires at least one K-line bar");
    auto program = parse_formula_program(source);
    auto environment = FormulaEnvironmentBuilder(
        kline_document, program, bars, parameters, context).build();
    auto& env = environment.numeric;
    auto& string_env = environment.strings;
    auto actual = std::move(environment.parameters);
    auto context_names = std::move(environment.context_bindings);
    const bool uses_random = environment.uses_random;
    const auto random_seed = environment.random_seed;
    const auto& random_seed_mode = environment.random_seed_mode;
    std::map<std::string, Series, std::less<>> outputs;
    Json render_primitives = Json::array();
    Json trade_event_primitives = Json::array();
    std::uint64_t render_event_count = 0;
    const auto number_format = native_annotation_number_format(kline_document);
    for (std::size_t statement_index = 0;
        statement_index < program.statements.size(); ++statement_index) {
        const auto& statement = program.statements[statement_index];
        auto trade_event = evaluate_trade_event_statement(
            statement, env, string_env, bars.size(), statement_index);
        auto value = trade_event
            ? std::move(trade_event->value)
            : evaluate_node(*statement.expression, env, string_env, bars.size());
        env[statement.name] = value;
        StringSeries string_value;
        if (evaluate_string_node(*statement.expression, env, string_env,
                                 bars.size(), string_value))
            string_env[statement.name] = std::move(string_value);
        auto primitive = render_primitive_document(
            statement, env, string_env, bars, number_format, statement_index,
            render_primitives.size());
        if (!primitive.is_null()) {
            if (const auto* count = optional(primitive, "event_count");
                count && count->is_number())
                render_event_count += static_cast<std::uint64_t>(count->as_number());
            render_primitives.push_back(std::move(primitive));
        }
        if (trade_event)
            trade_event_primitives.push_back(std::move(trade_event->primitive));
        if (statement.output) outputs[statement.name] = std::move(value);
    }
    if (outputs.empty() && !program.statements.empty()) {
        const auto& last = program.statements.back(); outputs[last.name] = env.at(last.name);
    }
    Json output_names = Json::array(); for (const auto& name : program.outputs) if (outputs.count(name)) output_names.push_back(name);
    Json points = Json::array();
    for (std::size_t i = 0; i < bars.size(); ++i) {
        Json point = Json::object(); point["date"] = bars[i].date; point["time"] = bars[i].time;
        point["open"] = bars[i].open; point["high"] = bars[i].high; point["low"] = bars[i].low;
        point["close"] = bars[i].close;
        point["amount"] = std::isfinite(bars[i].amount) ? Json(bars[i].amount) : Json(nullptr);
        point["volume"] = bars[i].volume;
        if (std::isfinite(bars[i].open_interest)) point["open_interest"] = bars[i].open_interest;
        if (std::isfinite(bars[i].hk_short_volume)) point["hk_short_volume"] = bars[i].hk_short_volume;
        Json values = Json::object(); for (const auto& [name, series] : outputs) values[name] = std::isfinite(series[i]) ? Json(series[i]) : Json(nullptr);
        point["values"] = std::move(values); points.push_back(std::move(point));
    }
    Json result = Json::object(); result["schema_version"] = 1;
    result["engine"] = "tdx-source-interpreter-v1"; result["execution_mode"] = "native-cpp";
    result["formula"] = formula_code; result["parameters"] = std::move(actual);
    result["context_bindings"] = std::move(context_names);
    Json context_metadata = Json::object();
    if (context && context->is_object())
        for (const auto key : {"professional_finance_mode", "professional_finance_report_date",
                               "finance_growth_mode", "finance_growth_report_date",
                               "finance_growth_values", "finance_event_mode", "finance_event_today",
                               "finance_point_in_time_mode", "finance_point_in_time_archive",
                               "finance_point_in_time_cache",
                               "finance_point_in_time_archive_event_count",
                               "finance_point_in_time_loaded_report_count",
                               "finance_point_in_time_failed_report_count",
                               "finance_point_in_time_failures",
                               "finance_point_in_time_truncated",
                               "finance_point_in_time_same_day_available",
                               "finance_point_in_time_first_available_from",
                               "finance_point_in_time_last_available_from",
                               "finance_event_values", "finance_eligibility_mode",
                               "finance_market_mode", "finance_market_id",
                               "finance_supported_selectors", "finance_source",
                               "finance_selector_evidence",
                               "finance_eligibility_source", "finance_eligibility_values",
                               "finance_eligibility_margin_count",
                               "finance_eligibility_stock_connect_count", "benchmark",
                               "benchmark_mode",
                               "beta_benchmark",
                               "main_index_identity", "underlying_identity",
                               "divfactor_mode", "divfactor_front_semantics",
                               "divfactor_back_semantics",
                               "divfactor_event_date_count",
                               "divfactor_matched_bar_count",
                               "dynamic_quote_command", "dynamic_speed_command",
                               "dynamic_alias_mode", "dynamic_aliases", "dynainfo39_mode",
                               "dynainfo_cage_mode",
                               "dynainfo_cage_values", "dynainfo_limit_mode",
                               "dynainfo_limit_values", "dynainfo_seal_state_mode",
                               "dynainfo_seal_state_values",
                               "external_security_series_mode", "external_security_series",
                                "formula_external_signal_mode",
                                "formula_external_signal_record_format",
                                "formula_external_signal_namespace_selector",
                                "formula_external_signal_user_path",
                                "formula_external_signal_system_path",
                                "formula_external_signal_user_exists",
                                "formula_external_signal_system_exists",
                                "formula_external_signal_loaded_record_count",
                                "formula_external_signal_matched_binding_count",
                                "formula_external_signal_missing_numeric",
                                "formula_external_signal_missing_text",
                                "formula_external_series_mode",
                                "formula_external_series_missing_modes",
                                "formula_external_series_host_point_limit",
                                "formula_external_series",
                                "formula_local_signal_mode",
                                "formula_local_signal_missing_modes",
                                "formula_local_signal_host_point_limit",
                                "formula_local_signals",
                                "industry_index_series_mode", "industry_index",
                                "industry_valuation",
                                "formula_text_symbols", "formula_text_symbols_mode",
                                "formula_text_symbol_sources",
                                "level1_research_industry",
                                "more_industry",
                                "main_business_source", "main_business_record_count",
                                "main_business_mode",
                                "security_score_source", "security_score_record_count",
                                "security_score_mode", "security_score_values",
                                "custom_block_directory_source",
                                "custom_block_directory_count",
                                "custom_block_membership_count",
                                "custom_block_mode",
                                "combination_block_directory_source",
                                "combination_block_directory_count",
                                "combination_block_readable_member_file_count",
                                "combination_block_membership_count",
                                "combination_block_mode",
                                "similar_block_mode",
                                "type167_block_code_mode",
                                "type167_block_code_limit",
                                "type167_concept_code_count",
                                "type167_style_code_count",
                                "type167_block_code_total",
                                "type167_concept_codes",
                                "type167_style_codes",
                                "code_name_lookup_mode",
                                "code_name_override_count",
                                "code_name_security_catalog_source",
                                "blocksetnum_resolutions",
                                "blocksetnum_binding_count",
                                "blocksetnum_industry_mode",
                                "blocksetnum_custom_catalog_source",
                                "blocksetnum_custom_directory_count",
                                "blocksetnum_mode",
                                "horcalc_resolutions",
                                "horcalc_binding_count",
                                "horcalc_member_file_count",
                                "horcalc_member_series_count",
                                "horcalc_finance_record_count",
                                "horcalc_quote_record_count",
                                "horcalc_industry_mode",
                                "horcalc_custom_catalog_source",
                                "horcalc_mode",
                                "indicator_aggregate_resolutions",
                                "insort_binding_count",
                                "insum_binding_count",
                                "indicator_aggregate_member_file_count",
                                "indicator_aggregate_member_series_count",
                                "indicator_aggregate_formula_evaluation_count",
                                "indicator_aggregate_warmup_bars",
                                "indicator_aggregate_industry_mode",
                                "indicator_aggregate_custom_catalog_source",
                                "indicator_aggregate_mode",
                                "calcstockindex_resolutions",
                                "calcstockindex_binding_count",
                                "calcstockindex_maximum_depth",
                                "calcstockindex_warmup_bars",
                                "calcstockindex_mode",
                                "formula_reference_resolutions",
                                "formula_reference_binding_count",
                                "formula_reference_evaluation_count",
                                "formula_reference_maximum_depth",
                                "formula_reference_mode",
                                "finone_mode", "finone_period_count",
                               "finone_relative_period_limit", "finone_cache",
                               "gpjyone_mode", "gpjyone_record_count",
                               "bkjyone_mode", "bkjyone_target_market",
                               "bkjyone_target_code", "bkjyone_record_count",
                               "scjyone_mode", "scjyone_record_count",
                               "gponedat_mode", "gponedat_source",
                               "gponedat_source_exists", "gponedat_record_count",
                               "gponedat_match_count",
                               "capital_series_mode", "capital_series_event_count",
                               "capital_series_oldest_event", "capital_series_newest_event",
                               "split_series_mode", "split_series_event_count",
                               "split_series_binding_count", "split_series_oldest_event",
                               "split_series_newest_event", "security_status_mode",
                               "security_status_values", "security_status_spblock_source",
                               "security_status_t0_fund_count",
                               "security_status_beijing_convertible_bond_count",
                               "security_status_quit_source",
                               "security_status_quit_effective_date",
                               "security_status_quit_active_count",
                               "security_status_category",
                               "security_status_category_source",
                               "contract_multiplier", "contract_multiplier_raw",
                               "contract_multiplier_category",
                               "contract_multiplier_mode",
                               "contract_multiplier_source",
                               "security_stat_functions",
                               "public_market_summary_functions",
                               "host_calendar_mode", "host_calendar_values",
                               "host_calendar_machine_date",
                               "host_calendar_current_trading_date",
                               "host_calendar_current_trading_date_source",
                               "host_calendar_local_day_file",
                               "host_calendar_local_day_file_exists",
                               "host_calendar_local_day_record_count",
                               "host_calendar_local_day_last_date",
                               "host_calendar_local_day_synthetic_current",
                               "host_calendar_local_day_effective_count"})
            if (const auto* value = optional(*context, key)) context_metadata[key] = *value;
    result["context_metadata"] = std::move(context_metadata);
    if (uses_random) {
        Json random = Json::object();
        random["algorithm"] = "msvc-crt-rand-lcg-v1";
        random["seed"] = static_cast<std::uint64_t>(random_seed);
        random["seed_mode"] = random_seed_mode;
        random["seed_scope"] = "formula-evaluation;reset-per-RAND-call";
        random["invalid_input_advances_state"] = false;
        result["random"] = std::move(random);
    }
    result["outputs"] = std::move(output_names); result["count"] = static_cast<std::uint64_t>(bars.size());
    Json render_ir = Json::object();
    render_ir["schema_version"] = 1;
    render_ir["schema"] = "tdx-formula-render-ir-v1";
    render_ir["bar_reference"] = "points[index]";
    render_ir["primitive_count"] = static_cast<std::uint64_t>(render_primitives.size());
    render_ir["event_count"] = render_event_count;
    render_ir["directive_order_preserved"] = true;
    render_ir["primitive_order"] = "source-statement-order";
    render_ir["primitive_order_contiguous"] = true;
    render_ir["source_statement_count"] =
        static_cast<std::uint64_t>(program.statements.size());
    render_ir["string_expressions_materialized"] = true;
    render_ir["rgb_encoding"] = "Windows COLORREF: red | green<<8 | blue<<16";
    render_ir["pixel_renderer_equivalent"] = false;
    render_ir["primitives"] = std::move(render_primitives);
    result["render_ir"] = std::move(render_ir);
    auto autofilter_projection = apply_autofilter_projection(
        program, trade_event_primitives, bars.size());
    result["trade_event_ir"] = trade_event_ir_document(
        std::move(trade_event_primitives), program.statements.size(),
        std::move(autofilter_projection));
    std::vector<std::string> parameter_names;
    for (const auto& [name, value] : parameters) { (void)value; parameter_names.push_back(name); }
    result["points"] = std::move(points);
    result["analysis"] = analyze_formula_source(source, parameter_names);
    for (const auto key : {"market", "code", "name", "source", "period", "period_id", "endpoint",
                           "server_name", "index_mode", "expansion_market", "transport", "volume_unit",
                           "open_interest_field", "auxiliary_field", "start", "next_start", "has_more",
                           "downloaded", "page_size", "adjustment_mode", "adjustment"})
        if (const auto* value = optional(kline_document, key)) result[key] = *value;
    const auto* index = optional(kline_document, "index_mode"); const bool index_mode = index && index->is_bool() && index->as_bool();
    const auto* expansion = optional(kline_document, "expansion_market");
    const bool expansion_market = expansion && expansion->is_bool() && expansion->as_bool();
    result["formula_volume_unit"] = expansion_market ? "contract" : index_mode ? "index-native" : "hand";
    result["formula_volume_divisor"] = index_mode || expansion_market ? 1 : 100; result["chronological"] = true;
    return result;
}

Json evaluate_formula_document(Json kline_document, const Json& formula,
                               const std::map<std::string, double>& parameters,
                               const Json* context) {
    const auto* source = optional(formula, "source_text");
    if (!source || !source->is_string()) throw Error("selected formula source text is unavailable");
    auto merged = effective_formula_parameters(formula, parameters);
    return evaluate_formula_source_document(std::move(kline_document), source->as_string(), merged,
                                            formula.at("code").as_string(), context);
}

Json audit_formula_library_document(Json kline_document, Json library, const Json* context,
                                    bool allow_future) {
    library = analyze_formula_library_document(std::move(library));
    Json formulas = Json::array();
    std::uint64_t eligible = 0, future_eligible = 0, passed = 0, errors = 0,
                  any_numeric = 0, latest_numeric = 0, market_inapplicable = 0,
                  period_inapplicable = 0, dependency_unavailable = 0,
                  context_unavailable = 0, future_read_only_disabled = 0;
    bool has_open_interest = false, has_hk_short_volume = false;
    const auto* expansion_value = optional(kline_document, "expansion_market");
    const bool expansion_market = expansion_value && expansion_value->is_bool() &&
                                  expansion_value->as_bool();
    int expansion_market_id = -1;
    if (expansion_market) {
        if (const auto* market = optional(kline_document, "market");
            market && market->is_string()) {
            try {
                std::size_t used = 0;
                expansion_market_id = std::stoi(market->as_string(), &used);
                if (used != market->as_string().size()) expansion_market_id = -1;
            } catch (...) { expansion_market_id = -1; }
        }
    }
    const bool option_market = expansion_market_id == 4 || expansion_market_id == 5 ||
                               expansion_market_id == 6 || expansion_market_id == 7 ||
                               expansion_market_id == 67;
    if (const auto* bars = optional(kline_document, "bars"); bars && bars->is_array())
        for (const auto& bar : bars->as_array()) {
            const auto* open_interest = optional(bar, "open_interest");
            const auto* hk_short_volume = optional(bar, "hk_short_volume");
            has_open_interest = has_open_interest ||
                (open_interest && open_interest->is_number() &&
                 std::isfinite(open_interest->as_number()));
            has_hk_short_volume = has_hk_short_volume ||
                (hk_short_volume && hk_short_volume->is_number() &&
                 std::isfinite(hk_short_volume->as_number()));
        }
    for (const auto& formula : library.at("formulas").as_array()) {
        const auto& analysis = formula.at("analysis");
        Json row = Json::object();
        for (const auto key : {"code", "name", "kind_key", "kind_name"})
            if (const auto* value = optional(formula, key)) row[key] = *value;
        const bool future_read_only_capable =
            analysis.at("read_only_future_executable").as_bool();
        std::set<std::string> unsupported_expansion_dependencies;
        std::set<std::string> unsupported_expansion_bindings;
        bool needs_ivolat = false;
        const bool supported_hk_finance = expansion_market &&
            formula_context_detail::is_tcalc_hk_finance_market(
                expansion_market_id) &&
            formula_context_detail::supports_tcalc_hk_finance_analysis(
                analysis, expansion_market_id);
        if (const auto* dependencies = optional(analysis, "external_dependencies");
            dependencies && dependencies->is_array())
            for (const auto& dependency : dependencies->as_array()) {
                const auto name = dependency.as_string();
                needs_ivolat = needs_ivolat || name == "IVOLAT";
                if (expansion_market && !expansion_context_dependency(name) &&
                    !(name == "FINANCE" && supported_hk_finance))
                    unsupported_expansion_dependencies.insert(name);
            }
        if (expansion_market &&
            formula_context_detail::is_tcalc_hk_finance_market(
                expansion_market_id)) {
            if (const auto* bindings = optional(
                    analysis, "context_bindings_required");
                bindings && bindings->is_array())
                for (const auto& binding : bindings->as_array()) {
                    if (!binding.is_string()) continue;
                    const auto selector =
                        formula_context_detail::tcalc_finance_binding_selector(
                            binding.as_string());
                    if (selector &&
                        !formula_context_detail::is_tcalc_hk_finance_selector(
                            expansion_market_id, *selector))
                        unsupported_expansion_bindings.insert(
                            binding.as_string());
                }
        }
        if (expansion_market &&
            (!unsupported_expansion_dependencies.empty() ||
             (needs_ivolat && !option_market))) {
            row["status"] = "market_inapplicable";
            if (!unsupported_expansion_dependencies.empty())
                row["unsupported_expansion_dependencies"] =
                    strings_json(unsupported_expansion_dependencies);
            if (!unsupported_expansion_bindings.empty())
                row["unsupported_expansion_bindings"] =
                    strings_json(unsupported_expansion_bindings);
            if (needs_ivolat && !option_market)
                row["required_market_context"] =
                    strings_json(std::set<std::string>{"ivolat"});
            ++market_inapplicable;
            formulas.push_back(std::move(row));
            continue;
        }
        const auto missing_context_bindings =
            missing_materialized_context_bindings(analysis, context);
        const bool context_bindings_ready = missing_context_bindings.empty();
        const bool automatic_context_executable =
            automatic_formula_context_enabled(context) &&
            analysis.at("executable_with_context").as_bool() &&
            context_bindings_ready;
        const bool explicit_context_executable =
            explicit_formula_context_ready(analysis, context);
        const bool normal_executable = analysis.at("executable").as_bool() ||
            (context_bindings_ready &&
             (automatic_context_executable || explicit_context_executable));
        const bool future_executable = allow_future &&
            future_read_only_capable &&
            (!analysis.at("has_external_dependency").as_bool() || context) &&
            context_bindings_ready;
        const bool executable = normal_executable || future_executable;
        row["future_read_only_capable"] = future_read_only_capable;
        row["future_read_only"] = future_executable;
        row["explicit_context"] = explicit_context_executable;
        if (!executable) {
            if (future_read_only_capable && !allow_future) {
                row["status"] = "future_read_only_disabled";
                ++future_read_only_disabled;
            } else if (!missing_context_bindings.empty() && context &&
                       automatic_formula_context_enabled(context) &&
                       analysis.at("executable_with_context").as_bool()) {
                row["status"] = "context_unavailable";
                row["context_bindings_required"] =
                    analysis.at("context_bindings_required");
                row["context_bindings_unavailable"] =
                    strings_json(missing_context_bindings);
                ++context_unavailable;
            } else if (!context && analysis.at("executable_with_context").as_bool()) {
                row["status"] = "context_unavailable";
                if (!missing_context_bindings.empty()) {
                    row["context_bindings_required"] =
                        analysis.at("context_bindings_required");
                    row["context_bindings_unavailable"] =
                        strings_json(missing_context_bindings);
                }
                ++context_unavailable;
            } else if (const auto* explicit_bindable = optional(
                           analysis, "explicit_context_bindable");
                       explicit_bindable && explicit_bindable->is_bool() &&
                       explicit_bindable->as_bool()) {
                row["status"] = "explicit_context_unavailable";
                ++context_unavailable;
            } else {
                row["status"] = "dependency_unavailable";
                ++dependency_unavailable;
            }
            for (const auto key : {"outputs", "external_dependencies", "unsupported",
                                   "context_bindings_required", "context_bindings_unavailable",
                                   "explicit_context_bindings_required"})
                if (!row.as_object().count(key))
                    if (const auto* value = optional(analysis, key))
                        row[key] = *value;
            formulas.push_back(std::move(row));
            continue;
        }
        bool needs_open_interest = false, needs_hk_short_volume = false;
        bool needs_ivolat_context = false;
        if (const auto* dependencies = optional(analysis, "expansion_market_dependencies");
            dependencies && dependencies->is_array())
            for (const auto& dependency : dependencies->as_array()) {
                needs_open_interest = needs_open_interest || dependency.as_string() == "VOLINSTK" ||
                                      dependency.as_string() == "CCL";
                needs_hk_short_volume = needs_hk_short_volume || dependency.as_string() == "HKSHORTVOL";
            }
        if (const auto* dependencies = optional(analysis, "external_dependencies");
            dependencies && dependencies->is_array())
            for (const auto& dependency : dependencies->as_array())
                needs_ivolat_context = needs_ivolat_context ||
                                       dependency.as_string() == "IVOLAT";
        const bool has_ivolat_context = context &&
            optional(*context, "ivolat") && optional(*context, "ivolat")->is_object();
        if ((needs_open_interest && !has_open_interest) ||
            (needs_hk_short_volume && !has_hk_short_volume) ||
            (needs_ivolat_context && !has_ivolat_context)) {
            std::set<std::string> required_market_context;
            if (needs_open_interest && !has_open_interest)
                required_market_context.insert("open_interest");
            if (needs_hk_short_volume && !has_hk_short_volume)
                required_market_context.insert("hk_short_volume");
            if (needs_ivolat_context && !has_ivolat_context)
                required_market_context.insert("ivolat");
            row["status"] = "market_inapplicable";
            row["required_market_context"] = strings_json(required_market_context);
            ++market_inapplicable;
            formulas.push_back(std::move(row));
            continue;
        }
        ++eligible; if (future_executable) ++future_eligible;
        try {
            const auto result = evaluate_formula_document(kline_document, formula, {}, context);
            std::set<std::string> numeric_outputs, latest_outputs;
            const auto& points = result.at("points").as_array();
            for (std::size_t i = 0; i < points.size(); ++i) {
                const auto& values = points[i].at("values").as_object();
                for (const auto& [name, value] : values) if (value.is_number()) {
                    numeric_outputs.insert(name);
                    if (i + 1 == points.size()) latest_outputs.insert(name);
                }
            }
            row["outputs"] = result.at("outputs");
            row["numeric_outputs"] = strings_json(numeric_outputs);
            row["latest_numeric_outputs"] = strings_json(latest_outputs);
            row["has_numeric_output"] = !numeric_outputs.empty();
            row["has_latest_numeric_output"] = !latest_outputs.empty();
            const auto* source = optional(formula, "source_text");
            const auto* period = optional(kline_document, "period");
            const bool minute_period = period && period->is_string() &&
                (lower_ascii(period->as_string()) == "1m" ||
                 lower_ascii(period->as_string()) == "time");
            const bool known_minute_only = numeric_outputs.empty() && !minute_period &&
                source && source->is_string() &&
                source->as_string().find("PERIOD=0") != std::string::npos;
            if (known_minute_only) {
                row["status"] = "period_inapplicable";
                row["required_period"] = "1m";
                ++period_inapplicable;
                --eligible;
                if (future_executable) --future_eligible;
            } else {
                row["status"] = "passed";
                ++passed;
                if (!numeric_outputs.empty()) ++any_numeric;
                if (!latest_outputs.empty()) ++latest_numeric;
            }
        } catch (const std::exception& error) {
            row["status"] = "error"; row["error"] = error.what(); ++errors;
        }
        formulas.push_back(std::move(row));
    }
    Json summary = Json::object();
    summary["library_total"] = static_cast<std::uint64_t>(library.at("formulas").as_array().size());
    summary["eligible"] = eligible; summary["passed"] = passed; summary["errors"] = errors;
    summary["future_read_only_eligible"] = future_eligible;
    summary["market_inapplicable"] = market_inapplicable;
    summary["period_inapplicable"] = period_inapplicable;
    summary["dependency_unavailable"] = dependency_unavailable;
    summary["context_unavailable"] = context_unavailable;
    summary["future_read_only_disabled"] = future_read_only_disabled;
    summary["with_numeric_output"] = any_numeric;
    summary["with_latest_numeric_output"] = latest_numeric;
    summary["without_numeric_output"] = passed - any_numeric;
    summary["without_latest_numeric_output"] = passed - latest_numeric;
    const auto library_total = static_cast<std::uint64_t>(library.at("formulas").as_array().size());
    const auto reported = static_cast<std::uint64_t>(formulas.as_array().size());
    summary["reported"] = reported;
    summary["unreported"] = library_total - reported;
    Json result = Json::object(); result["schema_version"] = 2;
    result["engine"] = "tdx-source-interpreter-v1";
    const bool automatic_context = automatic_formula_context_enabled(context);
    result["audit_mode"] = context
        ? (automatic_context
            ? (allow_future ? "market-context+future-read-only" : "market-context")
            : (allow_future ? "explicit-context+future-read-only" : "explicit-context"))
        : (allow_future ? "pure-ohlcv+future-read-only" : "pure-ohlcv");
    result["formula_count"] = library_total;
    result["reported_formula_count"] = reported;
    result["audit"] = std::move(summary);
    result["coverage"] = library.at("coverage"); result["formulas"] = std::move(formulas);
    for (const auto key : {"market", "code", "name", "period", "count", "source",
                           "expansion_market", "market_id", "adjustment_mode",
                           "adjustment"})
        if (const auto* value = optional(kline_document, key)) result[key] = *value;
    if (const auto* value = optional(kline_document, "count")) result["bar_count"] = *value;
    return result;
}

}  // namespace tdx
