#include "formula_analysis_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <set>
#include <utility>
#include <vector>

namespace tdx {

using namespace formula_analysis_detail;
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_render_detail;

Json analyze_formula_source(const std::string& source,
                            const std::vector<std::string>& parameters) {
    Json result = Json::object();
    result["source_available"] = !trim(source).empty();
    if (trim(source).empty()) {
        result["syntax_supported"] = false; result["executable"] = false;
        result["context_bindable"] = false; result["executable_with_context"] = false;
        result["explicit_context_bindable"] = false;
        result["read_only_future_executable"] = false;
        result["numeric_signal_safe"] = false;
        result["presentation_semantics_faithful"] = false;
        result["render_semantics_materialized"] = false;
        result["string_semantics_faithful"] = false;
        result["render_ir_available"] = false;
        result["render_ir_schema_version"] = 1;
        result["pixel_renderer_equivalent"] = false;
        result["has_semantic_surrogate"] = false;
        result["has_presentation_return_surrogate"] = false;
        result["semantic_surrogate_scope"] = "none";
        result["has_degraded_numeric_output"] = false;
        result["pure_ohlcv"] = false; result["has_future_function"] = false;
        result["has_external_dependency"] = false; result["has_graphics"] = false;
        result["has_machine_clock_dependency"] = false;
        result["has_random_dependency"] = false;
        result["random_seed_bindable"] = false;
        result["random_functions"] = Json::array();
        result["has_adjustment_mode_dependency"] = false;
        result["requires_expansion_market_data"] = false;
        result["presentation_return_surrogates"] = Json::array();
        result["unsupported_presentation_directives"] = Json::array();
        result["unsupported_presentation_functions"] = Json::array();
        result["formula_reference_uses"] = Json::array();
        result["formula_reference_use_count"] = 0;
        result["parameterized_formula_reference_count"] = 0;
        result["scalar_parameterized_formula_reference_count"] = 0;
        result["parameterized_formula_reference_supported"] = false;
        result["reason"] = "source text is unavailable"; return result;
    }
    try {
        auto program = parse_formula_program(source);
        const bool has_machine_clock_dependency =
            std::any_of(program.functions.begin(), program.functions.end(),
                        [](const auto& name) {
                            return custom_formula_machine_clock_functions.count(name) != 0;
                        }) ||
            std::any_of(program.symbols.begin(), program.symbols.end(),
                        [](const auto& name) {
                            return custom_formula_machine_clock_functions.count(name) != 0;
                        });
        std::set<std::string> random_functions;
        for (const auto& name : program.functions)
            if (custom_formula_random_functions.count(name))
                random_functions.insert(name);
        const bool has_random_dependency = !random_functions.empty();
        const bool has_adjustment_mode_dependency =
            program.functions.count("TQFLAG") != 0 ||
            program.symbols.count("TQFLAG") != 0 ||
            program.functions.count("DIVFACTOR") != 0;
        const auto semantic = audit_program_semantics(program);
        std::set<std::string> unsupported_presentation_directives;
        for (const auto& directive : program.directives)
            if (!supported_presentation_directive(directive))
                unsupported_presentation_directives.insert(directive);
        std::set<std::string> unsupported_presentation_functions;
        for (const auto& function : semantic.presentation_functions)
            if (!presentation_degraded_functions.count(function))
                unsupported_presentation_functions.insert(function);
        const bool has_render_function = std::any_of(
            program.functions.begin(), program.functions.end(), [](const auto& function) {
                return render_ir_functions.count(function) != 0;
            });
        const bool has_graphics = !program.directives.empty() || has_render_function;
        const bool presentation_semantics_faithful =
            unsupported_presentation_directives.empty() &&
            unsupported_presentation_functions.empty();
        const bool render_semantics_materialized =
            has_graphics && presentation_semantics_faithful;
        std::set<std::string> parameter_set;
        for (const auto& name : parameters) parameter_set.insert(upper_ascii(name));
        std::vector<FormulaReferenceUse> formula_reference_uses;
        for (const auto& statement : program.statements)
            collect_formula_reference_uses(
                *statement.expression, parameter_set, formula_reference_uses);
        std::set<std::string> unsupported, future, external, dependencies;
        for (const auto& function : program.functions) {
            const bool formula_reference =
                supported_formula_reference_dependency(function);
            if (future_functions.count(function)) future.insert(function);
            if (external_functions.count(function)) external.insert(function);
            if (formula_reference) {
                external.insert(function);
                dependencies.insert(function);
            }
            if (!supported_functions.count(function) && !formula_reference)
                unsupported.insert(function);
            if (function == "WINNER" || function == "COST" || function == "COSTEX" ||
                function == "PWINNER" || function == "LWINNER" || function == "PPART")
                dependencies.insert("CAPITAL");
            if (function == "TDXNVI" || function == "TDXPVI") {
                dependencies.insert("CLOSE"); dependencies.insert("VOL");
            }
            if (function == "TDXKDJ") {
                dependencies.insert("CLOSE"); dependencies.insert("HIGH");
                dependencies.insert("LOW");
            }
            if (function == "TDXBOLLM") dependencies.insert("CLOSE");
            if (function == "TDXBB" || function == "TDXWIDTH") dependencies.insert("CLOSE");
            if (function == "TDXASI") {
                dependencies.insert("OPEN"); dependencies.insert("HIGH");
                dependencies.insert("LOW"); dependencies.insert("CLOSE");
            }
            if (function == "TDXSAR") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
            }
            if (function == "TDXVTY") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
                dependencies.insert("CLOSE");
            }
            if (function == "TDXMSI") {
                dependencies.insert("ADVANCE"); dependencies.insert("DECLINE");
                external.insert("ADVANCE"); external.insert("DECLINE");
            }
            if (function == "TDXMCST") {
                dependencies.insert("CLOSE"); dependencies.insert("AMOUNT");
                dependencies.insert("VOL"); dependencies.insert("CAPITAL");
            }
            if (function == "LFS") {
                dependencies.insert("VOL"); dependencies.insert("CAPITAL");
            }
            if (function == "DATETOCUR") dependencies.insert("DATE");
            if (custom_formula_directional_bar_functions.count(function)) {
                dependencies.insert("OPEN"); dependencies.insert("HIGH");
                dependencies.insert("LOW"); dependencies.insert("CLOSE");
                dependencies.insert("VOL");
            }
            if (function == "TDXSSRP") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
                dependencies.insert("VOL"); dependencies.insert("CAPITAL");
            }
            if (function == "TDXPAV" || function == "TDXPAVE") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
                dependencies.insert("CLOSE"); dependencies.insert("VOL");
                dependencies.insert("CAPITAL");
            }
            if (function == "TDXNDB") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
                dependencies.insert("CLOSE");
            }
            if (function == "TDXSC") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
                dependencies.insert("CLOSE"); dependencies.insert("VOL");
            }
            if (function == "TDXXLPLBASE") dependencies.insert("CLOSE");
            if (function == "TDXZXNH") {
                dependencies.insert("HIGH"); dependencies.insert("LOW");
                dependencies.insert("CLOSE"); dependencies.insert("AMOUNT");
                dependencies.insert("VOL");
            }
            if (function == "ZTPRICE" || function == "DTPRICE") {
                dependencies.insert("HOST_TYPE120_SECURITY_CLASS_RAW");
                dependencies.insert("HOST_EVALUATOR_MARKET_WORD_RAW");
                external.insert("HOST_TYPE120_SECURITY_CLASS_RAW");
                external.insert("HOST_EVALUATOR_MARKET_WORD_RAW");
            }
        }
        for (const auto& symbol : program.symbols) {
            if (custom_formula_directional_bar_functions.count(symbol)) {
                future.insert(symbol);
                dependencies.insert("OPEN"); dependencies.insert("HIGH");
                dependencies.insert("LOW"); dependencies.insert("CLOSE");
                dependencies.insert("VOL");
                continue;
            }
            if (program.assigned.count(symbol) || parameter_set.count(symbol) || constants.count(symbol) || builtin_symbols.count(symbol)) continue;
            if (intrinsic_formula_symbols.count(symbol)) {
                dependencies.insert("CLOSE"); dependencies.insert("HIGH"); dependencies.insert("LOW");
            }
            else if (symbol.rfind("EXTERNAL#", 0) == 0) { dependencies.insert(symbol); external.insert(symbol); }
            else if (market_symbols.count(symbol)) dependencies.insert(symbol);
            else if (symbol == "CODE" || symbol == "STKNAME")
                dependencies.insert(symbol);
            else if (external_symbols.count(symbol) || explicit_context_symbols.count(symbol)) {
                dependencies.insert(symbol); external.insert(symbol);
            }
            else unsupported.insert(symbol);
        }
        std::set<std::string> required_bindings, unavailable_bindings;
        for (const auto& statement : program.statements)
            collect_numbered_context_calls(*statement.expression, required_bindings, unavailable_bindings);
        if (program.functions.count("ZTPRICE") ||
            program.functions.count("DTPRICE")) {
            required_bindings.insert("HOST_TYPE120_SECURITY_CLASS_RAW");
            required_bindings.insert("HOST_EVALUATOR_MARKET_WORD_RAW");
        }
        for (const auto& use : formula_reference_uses)
            if (!use.scalar_arguments)
                unavailable_bindings.insert(
                    "FORMULAREF#DYNAMIC#" + use.binding);
        for (const auto& binding : required_bindings) {
            const auto parts = split(binding, '#');
            bool available = false;
            if (binding == "MAINZSHQ#DYNAMIC" ||
                binding == "TOTALHQINFO#DYNAMIC" ||
                binding == "TOTALMMPAMO#DYNAMIC") {
                available = true;
            } else if (parts.size() == 3 && parts[0] == "MAINZSHQ") {
                (void)std::stoi(parts[1]);
                (void)std::stoi(parts[2]);
                available = true;
            } else if (parts.size() == 2 && parts[0] == "TOTALHQINFO") {
                (void)std::stoi(parts[1]);
                available = true;
            } else if (parts.size() == 2 && parts[0] == "TOTALMMPAMO") {
                const int field = std::stoi(parts[1]);
                available = field >= 1 && field <= 4;
            } else if (binding.rfind("BLOCKSETNUM#", 0) == 0) {
                available = binding.size() > std::string("BLOCKSETNUM#").size();
            } else if (parts.size() == 5 && parts[0] == "HORCALC") {
                const int item = std::stoi(parts[2]);
                const int calculation = std::stoi(parts[3]);
                const int weight = std::stoi(parts[4]);
                available = !parts[1].empty() && item >= 100 && item <= 106 &&
                            calculation >= 0 && calculation <= 2 &&
                            weight >= 0 && weight <= 4;
            } else if (parts.size() == 5 &&
                       (parts[0] == "INSORT" || parts[0] == "INSUM")) {
                const int output = std::stoi(parts[3]);
                const int mode = std::stoi(parts[4]);
                available = !parts[1].empty() && !parts[2].empty() &&
                    output >= 1 &&
                    (parts[0] == "INSORT" ? mode >= 0 && mode <= 1
                                            : mode >= 0 && mode <= 5);
            } else if (parts.size() == 4 &&
                       parts[0] == "CALCSTOCKINDEX") {
                const int output = std::stoi(parts[3]);
                available = !parts[1].empty() && !parts[2].empty() &&
                            output >= 1 && output <= 64;
            } else if (parts.size() == 2) {
                const auto id = std::stoi(parts[1]);
                if (parts[0] == "FINANCE") available = finance_context_ids.count(id) != 0;
                else if (parts[0] == "FINVALUE") available = id >= 0 && id <= 584;
                else if (parts[0] == "GPONEDAT") available = id >= 0 && id <= 32767;
                else available = dynainfo_context_ids.count(id) != 0;
            } else if (parts.size() == 4 &&
                       (parts[0] == "GPJYVALUE" || parts[0] == "BKJYVALUE" ||
                        parts[0] == "SCJYVALUE")) {
                const int id = std::stoi(parts[1]), field = std::stoi(parts[2]),
                          type = std::stoi(parts[3]);
                const int minimum_id = parts[0] == "BKJYVALUE" ? 5 : 1;
                const int maximum_id = parts[0] == "GPJYVALUE" ? 44 :
                                       parts[0] == "BKJYVALUE" ? 19 : 42;
                available = id >= minimum_id && id <= maximum_id && field >= 1 && field <= 2 &&
                            type >= 0 && type <= 2;
            } else if (parts.size() == 4 && parts[0] == "FINONE") {
                const int id = std::stoi(parts[1]), year = std::stoi(parts[2]),
                          mmdd = std::stoi(parts[3]);
                available = id >= 0 && id <= 584 && year >= 0 && year <= 9999 &&
                            mmdd >= 0 && mmdd <= 9999;
            } else if (parts.size() == 5 &&
                       (parts[0] == "GPJYONE" || parts[0] == "BKJYONE" ||
                        parts[0] == "SCJYONE")) {
                const int id = std::stoi(parts[1]), field = std::stoi(parts[2]),
                          year = std::stoi(parts[3]), mmdd = std::stoi(parts[4]);
                const int minimum_id = parts[0] == "BKJYONE" ? 5 : 1;
                const int maximum_id = parts[0] == "GPJYONE" ? 44 :
                                       parts[0] == "BKJYONE" ? 19 : 42;
                available = id >= minimum_id && id <= maximum_id &&
                            field >= 1 && field <= 2 && year >= 0 && year <= 9999 &&
                            mmdd >= 0 && mmdd <= 9999;
            } else if (parts.size() == 3 &&
                       (parts[0] == "SPLIT" || parts[0] == "SPLITBARS")) {
                const int occurrence = std::stoi(parts[1]), type = std::stoi(parts[2]);
                available = occurrence >= 0 && occurrence < 500 && type >= 0 &&
                            type <= (parts[0] == "SPLIT" ? 1 : 2);
            } else if (parts.size() == 3 &&
                       (parts[0] == "EXTDATA_USER" ||
                        parts[0] == "SIGNALS_SYS" ||
                        parts[0] == "SIGNALS_USER")) {
                (void)std::stoi(parts[1]);
                (void)std::stoi(parts[2]);
                available = true;
            }
            if (!available) unavailable_bindings.insert(binding);
        }
        std::set<std::string> explicit_bindings;
        for (const auto& symbol : program.symbols)
            if (explicit_context_symbols.count(symbol)) explicit_bindings.insert(symbol);
        for (const auto& function : program.functions)
            if (explicit_context_symbols.count(function))
                explicit_bindings.insert(function);
        for (const auto& binding : required_bindings)
            if (binding.rfind("SIGNALS_QS#", 0) == 0 ||
                binding.rfind("L2_AMO#", 0) == 0 ||
                binding.rfind("L2_VOL#", 0) == 0 ||
                binding.rfind("L2_VOLNUM#", 0) == 0)
                explicit_bindings.insert(binding);
        if (program.functions.count("ZTPRICE") ||
            program.functions.count("DTPRICE")) {
            explicit_bindings.insert("HOST_TYPE120_SECURITY_CLASS_RAW");
            explicit_bindings.insert("HOST_EVALUATOR_MARKET_WORD_RAW");
        }
        bool external_bindable = unavailable_bindings.empty();
        for (const auto& dependency : external)
            if (!context_external_dependencies.count(dependency) &&
                !supported_external_security_dependency(dependency) &&
                !supported_formula_reference_dependency(dependency))
                external_bindable = false;
        const bool context_bindable = !external.empty() && unsupported.empty() && future.empty() &&
                                      external_bindable;
        bool explicit_external_bindable = !explicit_bindings.empty();
        for (const auto& dependency : external)
            if (!explicit_context_dependency(dependency) &&
                !context_external_dependencies.count(dependency) &&
                !supported_external_security_dependency(dependency) &&
                !supported_formula_reference_dependency(dependency))
                explicit_external_bindable = false;
        for (const auto& dependency : unsupported)
            if (!explicit_context_dependency(dependency)) explicit_external_bindable = false;
        for (const auto& binding : unavailable_bindings)
            if (!explicit_bindings.count(binding)) explicit_external_bindable = false;
        const bool explicit_context_bindable = future.empty() &&
                                               explicit_external_bindable;
        const bool read_only_future_executable = !future.empty() && unsupported.empty() &&
                                                 external_bindable;
        std::set<std::string> expansion_dependencies;
        for (const auto& dependency : dependencies)
            if (dependency == "VOLINSTK" || dependency == "CCL" ||
                dependency == "HKSHORTVOL")
                expansion_dependencies.insert(dependency);
        std::set<std::string> automatic_context_dependencies;
        for (const auto& dependency : external)
            if (context_external_dependencies.count(dependency) ||
                supported_external_security_dependency(dependency) ||
                supported_formula_reference_dependency(dependency))
                automatic_context_dependencies.insert(dependency);
        result["syntax_supported"] = true;
        result["executable"] = unsupported.empty() && future.empty() && external.empty();
        result["context_bindable"] = context_bindable;
        result["executable_with_context"] = result.at("executable").as_bool() || context_bindable;
        result["explicit_context_bindable"] = explicit_context_bindable;
        result["read_only_future_executable"] = read_only_future_executable;
        result["numeric_signal_safe"] = semantic.degraded_numeric_outputs.empty();
        result["presentation_semantics_faithful"] = presentation_semantics_faithful;
        result["render_semantics_materialized"] = render_semantics_materialized;
        result["string_semantics_faithful"] =
            std::none_of(program.functions.begin(), program.functions.end(), [](const auto& function) {
                return string_surrogate_functions.count(function) != 0;
            });
        result["has_semantic_surrogate"] = !semantic.surrogates.empty();
        result["has_presentation_return_surrogate"] = !semantic.surrogates.empty();
        result["has_degraded_numeric_output"] =
            !semantic.degraded_numeric_outputs.empty();
        result["semantic_fidelity"] = !semantic.degraded_numeric_outputs.empty()
            ? "numeric-degraded"
            : render_semantics_materialized
                ? "numeric-safe-render-ir-materialized"
                : has_graphics ? "numeric-safe-presentation-degraded" : "numeric-safe";
        result["semantic_surrogate_scope"] = semantic.surrogates.empty()
            ? "none" : semantic.degraded_numeric_outputs.empty()
                ? "presentation-return-only" : "numeric-output-degraded";
        result["pure_ohlcv"] = result.at("executable").as_bool() &&
                               expansion_dependencies.empty() &&
                               !has_machine_clock_dependency &&
                               !has_random_dependency &&
                               !has_adjustment_mode_dependency &&
                               external.empty();
        result["requires_expansion_market_data"] = !expansion_dependencies.empty();
        result["expansion_market_dependencies"] = strings_json(expansion_dependencies);
        result["has_future_function"] = !future.empty();
        result["has_external_dependency"] = !external.empty();
        result["has_machine_clock_dependency"] = has_machine_clock_dependency;
        result["has_random_dependency"] = has_random_dependency;
        result["random_seed_bindable"] = has_random_dependency;
        result["random_functions"] = strings_json(random_functions);
        result["has_adjustment_mode_dependency"] = has_adjustment_mode_dependency;
        result["has_graphics"] = has_graphics;
        result["render_ir_available"] = result.at("has_graphics");
        result["render_ir_schema_version"] = 1;
        result["render_ir_input_fidelity"] =
            "evaluated numeric/string arguments plus ordered statement directives";
        result["pixel_renderer_equivalent"] = false;
        result["semantic_surrogates"] = strings_json(semantic.surrogates);
        result["presentation_return_surrogates"] = strings_json(semantic.surrogates);
        result["presentation_functions"] = strings_json(semantic.presentation_functions);
        result["unsupported_presentation_directives"] =
            strings_json(unsupported_presentation_directives);
        result["unsupported_presentation_functions"] =
            strings_json(unsupported_presentation_functions);
        result["presentation_only_outputs"] =
            strings_json(semantic.presentation_only_outputs);
        result["degraded_numeric_outputs"] =
            strings_json(semantic.degraded_numeric_outputs);
        Json degraded_causes = Json::object();
        for (const auto& [output, causes] : semantic.degraded_output_causes)
            degraded_causes[output] = strings_json(causes);
        result["degraded_numeric_output_causes"] = std::move(degraded_causes);
        result["functions"] = strings_json(program.functions);
        result["unsupported"] = strings_json(unsupported);
        result["future_functions"] = strings_json(future);
        result["external_dependencies"] = strings_json(external);
        Json formula_references = Json::array();
        std::uint64_t parameterized_formula_references = 0;
        std::uint64_t scalar_parameterized_formula_references = 0;
        for (const auto& use : formula_reference_uses) {
            Json item = Json::object();
            item["binding"] = use.binding;
            item["parameterized"] = !use.argument_expressions.empty();
            item["scalar_arguments"] = use.scalar_arguments;
            item["arguments"] = Json::array();
            for (const auto& expression : use.argument_expressions)
                item["arguments"].push_back(expression);
            if (!use.argument_expressions.empty()) {
                ++parameterized_formula_references;
                if (use.scalar_arguments)
                    ++scalar_parameterized_formula_references;
            }
            formula_references.push_back(std::move(item));
        }
        result["formula_reference_uses"] = std::move(formula_references);
        result["formula_reference_use_count"] =
            static_cast<std::uint64_t>(formula_reference_uses.size());
        result["parameterized_formula_reference_count"] =
            parameterized_formula_references;
        result["scalar_parameterized_formula_reference_count"] =
            scalar_parameterized_formula_references;
        result["parameterized_formula_reference_supported"] =
            parameterized_formula_references ==
            scalar_parameterized_formula_references;
        result["context_bindings_required"] = strings_json(required_bindings);
        result["context_bindings_unavailable"] = strings_json(unavailable_bindings);
        result["explicit_context_bindings_required"] = strings_json(explicit_bindings);
        result["automatic_context_dependencies"] =
            strings_json(automatic_context_dependencies);
        result["requires_automatic_market_context"] =
            !automatic_context_dependencies.empty();
        result["market_dependencies"] = strings_json(dependencies);
        result["directives"] = strings_json(program.directives);
        result["outputs"] = strings_json(program.outputs);
        result["statement_count"] = static_cast<std::uint64_t>(program.statements.size());
    } catch (const std::exception& error) {
        result["syntax_supported"] = false; result["executable"] = false;
        result["context_bindable"] = false; result["executable_with_context"] = false;
        result["explicit_context_bindable"] = false;
        result["read_only_future_executable"] = false;
        result["numeric_signal_safe"] = false;
        result["presentation_semantics_faithful"] = false;
        result["render_semantics_materialized"] = false;
        result["string_semantics_faithful"] = false;
        result["render_ir_available"] = false;
        result["render_ir_schema_version"] = 1;
        result["pixel_renderer_equivalent"] = false;
        result["has_semantic_surrogate"] = false;
        result["has_presentation_return_surrogate"] = false;
        result["semantic_surrogate_scope"] = "none";
        result["has_degraded_numeric_output"] = false;
        result["pure_ohlcv"] = false; result["has_future_function"] = false;
        result["has_external_dependency"] = false; result["has_graphics"] = false;
        result["has_machine_clock_dependency"] = false;
        result["has_random_dependency"] = false;
        result["random_seed_bindable"] = false;
        result["random_functions"] = Json::array();
        result["has_adjustment_mode_dependency"] = false;
        result["requires_expansion_market_data"] = false;
        result["presentation_return_surrogates"] = Json::array();
        result["unsupported_presentation_directives"] = Json::array();
        result["unsupported_presentation_functions"] = Json::array();
        result["formula_reference_uses"] = Json::array();
        result["formula_reference_use_count"] = 0;
        result["parameterized_formula_reference_count"] = 0;
        result["scalar_parameterized_formula_reference_count"] = 0;
        result["parameterized_formula_reference_supported"] = false;
        result["reason"] = error.what();
    }
    return result;
}

}  // namespace tdx
