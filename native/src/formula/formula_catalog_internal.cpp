#include "formula_catalog_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"

#include <vector>

namespace tdx::formula_context_detail {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string() : std::string{};
}

int integer_or(const Json& object, std::string_view key, int fallback) {
    const auto* value = optional(object, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : fallback;
}

bool boolean_or(const Json& object, std::string_view key, bool fallback) {
    const auto* value = optional(object, key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}

const Json& technical_formula(const Json& formula_library,
                              const std::string& requested,
                              std::string_view consumer) {
    const auto* formulas = optional(formula_library, "formulas");
    if (!formulas || !formulas->is_array())
        throw Error(std::string(consumer) +
                    " requires a recovered TCalc formula library");
    if (requested.empty())
        throw Error(std::string(consumer) +
                    " requires a non-empty indicator");
    const auto wanted = lower_ascii(requested);
    for (const auto& formula : formulas->as_array()) {
        if (lower_ascii(text_or(formula, "code")) != wanted) continue;
        const auto kind = lower_ascii(text_or(formula, "kind_key"));
        if (kind == "technical" || integer_or(formula, "kind", -1) == 0)
            return formula;
    }
    throw Error(std::string(consumer) +
                " indicator is absent from the recovered technical catalog: " +
                requested);
}

FormulaCatalogSelection validated_selection(
    const Json& selected, std::string output_name,
    FormulaSelectionPolicy policy, std::string_view consumer) {
    const auto* source = optional(selected, "source_text");
    if (!source || !source->is_string())
        throw Error(std::string(consumer) +
                    " selected indicator source is unavailable: " +
                    text_or(selected, "code"));
    std::vector<std::string> parameter_names;
    if (const auto* parameters = optional(selected, "parameters");
        parameters && parameters->is_array())
        for (const auto& parameter : parameters->as_array()) {
            const auto name = text_or(parameter, "name");
            if (!name.empty()) parameter_names.push_back(name);
        }
    auto analysis = analyze_formula_source(source->as_string(), parameter_names);
    const bool executable = boolean_or(analysis, "executable", false);
    const bool context_executable =
        boolean_or(analysis, "executable_with_context", false);
    const bool future_executable =
        boolean_or(analysis, "read_only_future_executable", false);
    const bool numeric_safe =
        boolean_or(analysis, "numeric_signal_safe", false);
    bool allowed = false;
    std::string requirement;
    if (policy == FormulaSelectionPolicy::context_free_ohlcv) {
        allowed = executable && boolean_or(analysis, "pure_ohlcv", false);
        requirement = "a context-free, numeric-safe OHLCV indicator";
    } else if (policy == FormulaSelectionPolicy::automatic_nested_context) {
        allowed = executable || context_executable || future_executable;
        requirement = "a numeric-safe indicator executable with automatic context";
    } else {
        allowed = (executable || context_executable) &&
                  !boolean_or(analysis, "has_future_function", false);
        if (const auto* dependencies = optional(analysis, "external_dependencies");
            dependencies && dependencies->is_array())
            for (const auto& dependency : dependencies->as_array())
                if (!dependency.is_string() ||
                    !formula_engine_support::supported_formula_reference_dependency(
                        dependency.as_string()))
                    allowed = false;
        requirement =
            "a numeric-safe, non-future indicator whose external inputs are formula references";
    }
    if (!numeric_safe || !allowed)
        throw Error(std::string(consumer) + " requires " + requirement +
                    ": " + text_or(selected, "code"));

    FormulaCatalogSelection result;
    result.definition = &selected;
    result.code = text_or(selected, "code");
    result.name = text_or(selected, "name");
    result.output_name = std::move(output_name);
    result.analysis = std::move(analysis);
    return result;
}

}  // namespace

FormulaCatalogSelection select_technical_formula(
    const Json& formula_library, const std::string& requested, int output,
    FormulaSelectionPolicy policy, std::string_view consumer) {
    if (output < 1)
        throw Error(std::string(consumer) +
                    " requires a non-empty indicator and output >= 1");
    const auto& selected = technical_formula(
        formula_library, requested, consumer);
    const auto* outputs = optional(selected, "outputs");
    if (!outputs || !outputs->is_array() ||
        output > static_cast<int>(outputs->size()) ||
        !outputs->as_array()[static_cast<std::size_t>(output - 1)].is_string())
        throw Error(std::string(consumer) +
                    " selected indicator output is unavailable: " + requested +
                    "#" + std::to_string(output));
    return validated_selection(
        selected,
        outputs->as_array()[static_cast<std::size_t>(output - 1)].as_string(),
        policy, consumer);
}

FormulaCatalogSelection select_technical_formula_output(
    const Json& formula_library, const std::string& requested,
    const std::string& output_name, FormulaSelectionPolicy policy,
    std::string_view consumer) {
    if (output_name.empty())
        throw Error(std::string(consumer) +
                    " requires a non-empty indicator output name");
    const auto& selected = technical_formula(
        formula_library, requested, consumer);
    const auto* outputs = optional(selected, "outputs");
    if (!outputs || !outputs->is_array())
        throw Error(std::string(consumer) +
                    " selected indicator outputs are unavailable: " + requested);
    const auto wanted = lower_ascii(output_name);
    for (const auto& output : outputs->as_array())
        if (output.is_string() && lower_ascii(output.as_string()) == wanted)
            return validated_selection(
                selected, output.as_string(), policy, consumer);
    throw Error(std::string(consumer) +
                " selected indicator output is unavailable: " + requested +
                "." + output_name);
}

}  // namespace tdx::formula_context_detail
