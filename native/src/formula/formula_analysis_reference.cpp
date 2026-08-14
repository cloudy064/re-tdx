#include "tdx/formula_engine.hpp"

#include "formula_engine_support_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {
namespace {

using formula_engine_support::formula_parameter_names;
using formula_engine_support::optional;
using formula_engine_support::supported_formula_reference_dependency;

struct TechnicalTarget {
    std::string code;
    std::string name;
    std::map<std::string, std::string> outputs;
    bool runtime_eligible{};
};

struct FormulaReference {
    std::string binding;
    std::string target_key;
    std::string requested_code;
    std::string requested_output;
    std::string resolved_code;
    std::string resolved_name;
    std::string resolved_output;
    std::string status;
    bool runtime_eligible{};
};

struct ReferencingFormula {
    std::string code;
    std::string name;
    std::string kind;
    std::string technical_key;
    std::vector<FormulaReference> references;
};

std::string text_or(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string() : std::string{};
}

bool boolean_or(const Json& object, std::string_view key, bool fallback = false) {
    const auto* value = optional(object, key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}

bool technical_formula(const Json& formula) {
    if (lower_ascii(text_or(formula, "kind_key")) == "technical") return true;
    const auto* kind = optional(formula, "kind");
    return kind && kind->is_number() && static_cast<int>(kind->as_number()) == 0;
}

Json formula_analysis(const Json& formula) {
    if (const auto* analysis = optional(formula, "analysis");
        analysis && analysis->is_object())
        return *analysis;
    const auto* source = optional(formula, "source_text");
    return analyze_formula_source(
        source && source->is_string() ? source->as_string() : std::string{},
        formula_parameter_names(formula));
}

bool reference_target_runtime_eligible(const Json& analysis) {
    if (!boolean_or(analysis, "numeric_signal_safe") ||
        boolean_or(analysis, "has_future_function") ||
        (!boolean_or(analysis, "executable") &&
         !boolean_or(analysis, "executable_with_context")))
        return false;
    const auto* dependencies = optional(analysis, "external_dependencies");
    if (!dependencies || !dependencies->is_array()) return true;
    for (const auto& dependency : dependencies->as_array())
        if (!dependency.is_string() ||
            !supported_formula_reference_dependency(dependency.as_string()))
            return false;
    return true;
}

std::pair<std::string, std::string> split_reference(
    const std::string& dependency) {
    constexpr std::string_view prefix = "EXTERNAL#";
    const auto reference = dependency.substr(prefix.size());
    const auto separator = reference.rfind('.');
    return {reference.substr(0, separator), reference.substr(separator + 1)};
}

std::vector<std::vector<std::string>> cyclic_components(
    const std::map<std::string, std::set<std::string>>& adjacency) {
    std::map<std::string, int> indices, lowlinks;
    std::set<std::string> on_stack;
    std::vector<std::string> stack;
    std::vector<std::vector<std::string>> result;
    int next_index = 0;
    std::function<void(const std::string&)> visit = [&](const std::string& node) {
        indices[node] = next_index;
        lowlinks[node] = next_index++;
        stack.push_back(node);
        on_stack.insert(node);
        const auto found = adjacency.find(node);
        if (found != adjacency.end()) {
            for (const auto& target : found->second) {
                if (!indices.count(target)) {
                    visit(target);
                    lowlinks[node] = std::min(lowlinks[node], lowlinks[target]);
                } else if (on_stack.count(target)) {
                    lowlinks[node] = std::min(lowlinks[node], indices[target]);
                }
            }
        }
        if (lowlinks[node] != indices[node]) return;
        std::vector<std::string> component;
        while (!stack.empty()) {
            auto member = stack.back();
            stack.pop_back();
            on_stack.erase(member);
            component.push_back(std::move(member));
            if (component.back() == node) break;
        }
        const bool self_cycle = component.size() == 1 &&
            found != adjacency.end() && found->second.count(node) != 0;
        if (component.size() > 1 || self_cycle) {
            std::sort(component.begin(), component.end());
            result.push_back(std::move(component));
        }
    };
    for (const auto& [node, unused] : adjacency) {
        (void)unused;
        if (!indices.count(node)) visit(node);
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace

Json analyze_formula_reference_graph(const Json& library) {
    const auto* formulas = optional(library, "formulas");
    if (!formulas || !formulas->is_array())
        throw Error("formula reference analysis requires a formula library");

    std::map<std::string, TechnicalTarget> targets;
    for (const auto& formula : formulas->as_array()) {
        if (!technical_formula(formula)) continue;
        const auto code = text_or(formula, "code");
        const auto key = lower_ascii(code);
        if (key.empty() || targets.count(key)) continue;
        const auto analysis = formula_analysis(formula);
        TechnicalTarget target;
        target.code = code;
        target.name = text_or(formula, "name");
        target.runtime_eligible = reference_target_runtime_eligible(analysis);
        const auto* outputs = optional(analysis, "outputs");
        if (!outputs || !outputs->is_array()) outputs = optional(formula, "outputs");
        if (outputs && outputs->is_array())
            for (const auto& output : outputs->as_array())
                if (output.is_string())
                    target.outputs.emplace(lower_ascii(output.as_string()),
                                           output.as_string());
        targets.emplace(key, std::move(target));
    }

    std::vector<ReferencingFormula> referencing;
    std::map<std::string, std::set<std::string>> adjacency;
    for (const auto& [key, target] : targets) {
        (void)target;
        adjacency[key];
    }
    std::uint64_t binding_count = 0;
    for (const auto& formula : formulas->as_array()) {
        const auto analysis = formula_analysis(formula);
        const auto* dependencies = optional(analysis, "external_dependencies");
        if (!dependencies || !dependencies->is_array()) continue;
        ReferencingFormula row;
        row.code = text_or(formula, "code");
        row.name = text_or(formula, "name");
        row.kind = text_or(formula, "kind_key");
        if (technical_formula(formula)) row.technical_key = lower_ascii(row.code);
        for (const auto& dependency : dependencies->as_array()) {
            if (!dependency.is_string() ||
                !supported_formula_reference_dependency(dependency.as_string()))
                continue;
            FormulaReference reference;
            reference.binding = dependency.as_string();
            const auto [requested_code, requested_output] =
                split_reference(reference.binding);
            reference.requested_code = requested_code;
            reference.requested_output = requested_output;
            reference.target_key = lower_ascii(requested_code);
            const auto target = targets.find(reference.target_key);
            if (target == targets.end()) {
                reference.status = "missing-formula";
            } else {
                reference.resolved_code = target->second.code;
                reference.resolved_name = target->second.name;
                const auto output = target->second.outputs.find(
                    lower_ascii(requested_output));
                if (output == target->second.outputs.end()) {
                    reference.status = "missing-output";
                } else {
                    reference.status = "resolved";
                    reference.resolved_output = output->second;
                    reference.runtime_eligible = target->second.runtime_eligible;
                    if (!row.technical_key.empty())
                        adjacency[row.technical_key].insert(reference.target_key);
                }
            }
            row.references.push_back(std::move(reference));
            ++binding_count;
        }
        if (!row.references.empty()) referencing.push_back(std::move(row));
    }

    const auto components = cyclic_components(adjacency);
    std::map<std::string, std::size_t> component_by_formula;
    for (std::size_t index = 0; index < components.size(); ++index)
        for (const auto& formula : components[index])
            component_by_formula[formula] = index;

    std::uint64_t parameterized_reference_count = 0;
    for (const auto& formula : formulas->as_array()) {
        const auto analysis = formula_analysis(formula);
        const auto* count = optional(
            analysis, "parameterized_formula_reference_count");
        if (count && count->is_number())
            parameterized_reference_count +=
                static_cast<std::uint64_t>(count->as_number());
    }

    Json report = Json::object();
    report["schema"] = "tdx-formula-reference-graph-v1";
    report["reference_form"] = "\"INDICATOR.OUTPUT\"";
    report["parameterized_reference_supported"] = true;
    report["parameterized_reference_scope"] =
        "constant arithmetic and declared parent parameters";
    report["parameterized_reference_count"] = parameterized_reference_count;
    report["formulas"] = Json::array();
    std::uint64_t resolved = 0, missing_formula = 0, missing_output = 0,
                  runtime_eligible = 0, executable = 0, blocked_by_cycle = 0;
    for (const auto& formula : referencing) {
        Json formula_json = Json::object();
        formula_json["code"] = formula.code;
        formula_json["name"] = formula.name;
        formula_json["kind"] = formula.kind;
        formula_json["references"] = Json::array();
        for (const auto& reference : formula.references) {
            Json reference_json = Json::object();
            reference_json["binding"] = reference.binding;
            reference_json["indicator"] = reference.requested_code;
            reference_json["output"] = reference.requested_output;
            reference_json["status"] = reference.status;
            bool in_cycle = false;
            if (reference.status == "resolved") {
                ++resolved;
                reference_json["resolved_indicator"] = reference.resolved_code;
                reference_json["resolved_name"] = reference.resolved_name;
                reference_json["resolved_output"] = reference.resolved_output;
                reference_json["runtime_eligible"] = reference.runtime_eligible;
                if (reference.runtime_eligible) ++runtime_eligible;
                const auto source_component =
                    component_by_formula.find(formula.technical_key);
                const auto target_component =
                    component_by_formula.find(reference.target_key);
                in_cycle = source_component != component_by_formula.end() &&
                           target_component != component_by_formula.end() &&
                           source_component->second == target_component->second;
                if (in_cycle) ++blocked_by_cycle;
                if (reference.runtime_eligible && !in_cycle) ++executable;
            } else if (reference.status == "missing-formula") {
                ++missing_formula;
            } else {
                ++missing_output;
            }
            reference_json["in_cycle"] = in_cycle;
            formula_json["references"].push_back(std::move(reference_json));
        }
        report["formulas"].push_back(std::move(formula_json));
    }

    report["cycles"] = Json::array();
    std::uint64_t cyclic_formula_count = 0;
    for (const auto& component : components) {
        Json row = Json::object();
        row["formula_codes"] = Json::array();
        for (const auto& key : component) {
            const auto found = targets.find(key);
            row["formula_codes"].push_back(
                found == targets.end() ? key : found->second.code);
        }
        row["formula_count"] = static_cast<double>(component.size());
        cyclic_formula_count += component.size();
        report["cycles"].push_back(std::move(row));
    }

    Json summary = Json::object();
    summary["library_formula_count"] = static_cast<double>(formulas->size());
    summary["technical_formula_count"] = static_cast<double>(targets.size());
    summary["referencing_formula_count"] = static_cast<double>(referencing.size());
    summary["binding_count"] = static_cast<double>(binding_count);
    summary["resolved_binding_count"] = static_cast<double>(resolved);
    summary["missing_formula_count"] = static_cast<double>(missing_formula);
    summary["missing_output_count"] = static_cast<double>(missing_output);
    summary["runtime_eligible_binding_count"] =
        static_cast<double>(runtime_eligible);
    summary["executable_binding_count"] = static_cast<double>(executable);
    summary["blocked_by_cycle_binding_count"] =
        static_cast<double>(blocked_by_cycle);
    summary["cycle_component_count"] = static_cast<double>(components.size());
    summary["cyclic_formula_count"] = static_cast<double>(cyclic_formula_count);
    summary["all_resolved"] =
        missing_formula == 0 && missing_output == 0;
    summary["acyclic"] = components.empty();
    report["summary"] = std::move(summary);
    return report;
}

}  // namespace tdx
