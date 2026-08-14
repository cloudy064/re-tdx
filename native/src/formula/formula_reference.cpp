#include "formula_reference_internal.hpp"

#include "formula_catalog_internal.hpp"
#include "formula_engine_support_internal.hpp"
#include "formula_language_internal.hpp"
#include "formula_nested_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string_view>

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

bool boolean_or(const Json& object, std::string_view key, bool fallback) {
    const auto* value = optional(object, key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}

struct FormulaReferenceBinding {
    std::string name;
    std::string formula;
    std::string output;
};

FormulaReferenceBinding parse_reference(const std::string& binding) {
    if (!formula_engine_support::supported_formula_reference_dependency(binding))
        throw Error("invalid formula output reference: " + binding);
    constexpr std::string_view prefix = "EXTERNAL#";
    const auto value = binding.substr(prefix.size());
    const auto separator = value.rfind('.');
    return {binding, value.substr(0, separator), value.substr(separator + 1)};
}

std::string stamp(const Json& row) {
    const auto date = text_or(row, "date");
    return date.empty() ? std::string{} : date + "|" + text_or(row, "time");
}

Json output_points(const Json& evaluation, const std::string& output,
                   std::uint64_t& finite_points) {
    const auto* rows = optional(evaluation, "points");
    if (!rows || !rows->is_array())
        throw Error("formula output reference evaluation has no points");
    Json result = Json::object();
    for (const auto& row : rows->as_array()) {
        const auto key = stamp(row);
        const auto* values = optional(row, "values");
        const auto* value = values ? optional(*values, output) : nullptr;
        if (key.empty() || !value || !value->is_number() ||
            !std::isfinite(value->as_number()))
            continue;
        result[key] = *value;
        ++finite_points;
    }
    return result;
}

}  // namespace

void bind_formula_reference_context(
    Json& context, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const Json& caller_kline,
    const std::vector<FormulaReferenceRequest>& raw_bindings,
    const Json& formula_library, int timeout_ms, const BlockData* block_data,
    const std::filesystem::path& jsn_root, const Json* nested_state,
    const std::map<std::string, double>& caller_parameters) {
    if (raw_bindings.empty()) return;
    if (!context.as_object().count("series")) context["series"] = Json::object();

    std::map<std::string, Json, std::less<>> evaluation_cache;
    std::set<std::string> bound_keys;
    Json resolutions = Json::array();
    for (const auto& raw : raw_bindings) {
        const auto binding = parse_reference(raw.binding);
        std::vector<double> argument_values;
        argument_values.reserve(raw.argument_expressions.size());
        for (const auto& expression : raw.argument_expressions)
            argument_values.push_back(
                formula_language_detail::evaluate_formula_scalar_expression(
                    expression, caller_parameters));
        const auto runtime_binding = argument_values.empty()
            ? binding.name
            : formula_engine_support::parameterized_formula_reference_binding(
                  binding.name, argument_values);
        if (!bound_keys.insert(runtime_binding).second) continue;
        const auto selection = select_technical_formula_output(
            formula_library, binding.formula, binding.output,
            FormulaSelectionPolicy::same_security_reference,
            "formula output reference");
        std::map<std::string, double> child_parameters;
        const auto* parameter_specs = optional(
            *selection.definition, "parameters");
        const std::size_t parameter_count =
            parameter_specs && parameter_specs->is_array()
                ? parameter_specs->size() : 0;
        if (argument_values.size() > parameter_count)
            throw Error("formula output reference supplies " +
                        std::to_string(argument_values.size()) +
                        " parameters but " + selection.code + " accepts " +
                        std::to_string(parameter_count));
        for (std::size_t index = 0; index < argument_values.size(); ++index) {
            const auto* name = optional(
                parameter_specs->as_array()[index], "name");
            if (!name || !name->is_string() || name->as_string().empty())
                throw Error("formula output reference target has an invalid parameter slot: " +
                            selection.code);
            child_parameters[name->as_string()] = argument_values[index];
        }
        const auto identity = lower_ascii(market) + ":" + code + ":" +
                              lower_ascii(selection.code);
        auto child_state = next_nested_formula_state(
            nested_state, identity, "formula output reference");
        const auto parameter_suffix = runtime_binding.substr(binding.name.size());
        const auto cache_key = lower_ascii(selection.code) + parameter_suffix;
        auto evaluated = evaluation_cache.find(cache_key);
        if (evaluated == evaluation_cache.end()) {
            Json child_context = Json::object();
            if (boolean_or(selection.analysis, "has_external_dependency", false)) {
                child_context = build_formula_market_context_document(
                    root, market, code, selection.analysis, timeout_ms,
                    block_data, &caller_kline, false, &formula_library,
                    jsn_root, &child_state, &child_parameters);
                child_context["automatic_market_context"] = true;
            }
            auto document = evaluate_formula_document(
                caller_kline, *selection.definition, child_parameters,
                child_context.size() ? &child_context : nullptr);
            evaluated = evaluation_cache.emplace(
                cache_key, std::move(document)).first;
        }

        std::uint64_t finite_points = 0;
        context["series"][runtime_binding] = output_points(
            evaluated->second, selection.output_name, finite_points);
        Json item = Json::object();
        item["binding"] = runtime_binding;
        item["reference"] = binding.name;
        item["formula"] = selection.code;
        item["formula_name"] = selection.name;
        item["output_name"] = selection.output_name;
        item["nested_depth"] = child_state.at("depth");
        item["finite_point_count"] = finite_points;
        item["parameter_values"] = Json::array();
        for (const double value : argument_values)
            item["parameter_values"].push_back(value);
        resolutions.push_back(std::move(item));
    }
    context["formula_reference_resolutions"] = std::move(resolutions);
    context["formula_reference_binding_count"] =
        static_cast<std::uint64_t>(bound_keys.size());
    context["formula_reference_evaluation_count"] =
        static_cast<std::uint64_t>(evaluation_cache.size());
    context["formula_reference_maximum_depth"] =
        maximum_nested_formula_depth;
    context["formula_reference_mode"] =
        "tdx-double-quoted-same-security-same-period-library-output-with-scalar-parameters";
}

}  // namespace tdx::formula_context_detail
