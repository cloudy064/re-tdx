#include "formula_analysis_internal.hpp"

#include <utility>

namespace tdx::formula_analysis_detail {

using namespace formula_engine_detail;
using namespace formula_language_detail;

bool explicit_context_dependency(std::string_view dependency) {
    return dependency == "SIGNALS_QS" || dependency == "L2_AMO" ||
           dependency == "L2_VOL" || dependency == "L2_VOLNUM" ||
           explicit_context_symbols.count(std::string(dependency)) != 0;
}

void merge_taint(SemanticTaint& target, const SemanticTaint& source) {
    target.insert(source.begin(), source.end());
}

SemanticTaint numeric_semantic_taint(
    const Node& node,
    const std::map<std::string, SemanticTaint, std::less<>>& assigned_taint) {
    if (node.kind == NodeKind::number) return {};
    if (node.kind == NodeKind::string_literal) return {"STRING_LITERAL"};
    if (node.kind == NodeKind::symbol) {
        const auto found = assigned_taint.find(node.text);
        if (found != assigned_taint.end()) return found->second;
        return string_symbols.count(node.text)
            ? SemanticTaint{node.text + "#STRING_HANDLE"} : SemanticTaint{};
    }
    if (node.kind == NodeKind::call) {
        if (numeric_surrogate_functions.count(node.text)) {
            SemanticTaint result{node.text};
            for (const auto& child : node.children)
                merge_taint(
                    result, numeric_semantic_taint(*child, assigned_taint));
            return result;
        }
        if (node.text == "BLOCKSETNUM" || node.text == "HORCALC" ||
            node.text == "CALCSTOCKINDEX" ||
            node.text == "INSORT" || node.text == "INSUM" ||
            node.text == "STR2CON" || node.text == "FINDSTR" ||
            node.text == "STRLEN" ||
            node.text == "NAMELIKE" || node.text == "CODELIKE" ||
            node.text == "NAMEINCLUDE")
            return {};
        if (node.text == "STRCMP") return {};
        // PARTLINE's first argument is the plotted numeric ordinate.  Its
        // remaining arguments are presentation attributes (colour/style).
        if (node.text == "PARTLINE") {
            return node.children.empty()
                ? SemanticTaint{"PARTLINE#MISSING_ORDINATE"}
                : numeric_semantic_taint(*node.children.front(), assigned_taint);
        }
    }
    SemanticTaint result;
    for (const auto& child : node.children)
        merge_taint(result, numeric_semantic_taint(*child, assigned_taint));
    return result;
}

SemanticAudit audit_program_semantics(const Program& program) {
    SemanticAudit result;
    for (const auto& function : program.functions) {
        if (numeric_surrogate_functions.count(function)) result.surrogates.insert(function);
        if (presentation_degraded_functions.count(function))
            result.presentation_functions.insert(function);
    }

    std::map<std::string, SemanticTaint, std::less<>> assigned_taint;
    SemanticTaint last_taint;
    bool last_presentation_only = false;
    std::string last_name;
    for (const auto& statement : program.statements) {
        const bool presentation_only = statement.expression->kind == NodeKind::call &&
            presentation_only_functions.count(statement.expression->text) != 0;
        SemanticTaint taint;
        if (presentation_only) taint.insert(statement.expression->text);
        else taint = numeric_semantic_taint(*statement.expression, assigned_taint);
        assigned_taint[statement.name] = taint;
        if (statement.output) {
            if (presentation_only) result.presentation_only_outputs.insert(statement.name);
            else if (!taint.empty()) {
                result.degraded_numeric_outputs.insert(statement.name);
                result.degraded_output_causes[statement.name] = taint;
            }
        }
        last_taint = std::move(taint);
        last_presentation_only = presentation_only;
        last_name = statement.name;
    }
    // The evaluator exposes the final assignment when a custom formula has no
    // declared output.  Audit that fallback exactly as the runtime does.
    if (program.outputs.empty() && !last_name.empty()) {
        if (last_presentation_only) result.presentation_only_outputs.insert(last_name);
        else if (!last_taint.empty()) {
            result.degraded_numeric_outputs.insert(last_name);
            result.degraded_output_causes[last_name] = last_taint;
        }
    }
    return result;
}

}  // namespace tdx::formula_analysis_detail
