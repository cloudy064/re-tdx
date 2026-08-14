#pragma once

#include "tdx/formula_engine.hpp"

#include "formula_engine_internal.hpp"
#include "formula_engine_support_internal.hpp"
#include "formula_language_internal.hpp"
#include "formula_render_support_internal.hpp"

#include <map>
#include <set>
#include <string>
#include <string_view>

namespace tdx::formula_analysis_detail {

using SemanticTaint = std::set<std::string>;

struct SemanticAudit {
    std::set<std::string> surrogates;
    std::set<std::string> presentation_functions;
    std::set<std::string> presentation_only_outputs;
    std::set<std::string> degraded_numeric_outputs;
    std::map<std::string, SemanticTaint, std::less<>> degraded_output_causes;
};

bool explicit_context_dependency(std::string_view dependency);
SemanticAudit audit_program_semantics(
    const formula_language_detail::Program& program);

}  // namespace tdx::formula_analysis_detail

