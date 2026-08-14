#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx::formula_context_detail {

struct FormulaReferenceRequest {
    std::string binding;
    std::vector<std::string> argument_expressions;
};

// Materialize TDX double-quoted `INDICATOR.OUTPUT` references against the same
// security and K-line window. Referenced indicators are selected from the
// recovered system+user library and recursively protected against cycles.
void bind_formula_reference_context(
    Json& context,
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const Json& caller_kline,
    const std::vector<FormulaReferenceRequest>& bindings,
    const Json& formula_library,
    int timeout_ms,
    const BlockData* block_data,
    const std::filesystem::path& jsn_root,
    const Json* nested_state,
    const std::map<std::string, double>& caller_parameters);

}  // namespace tdx::formula_context_detail
