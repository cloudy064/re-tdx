#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::formula_context_detail {

inline constexpr int maximum_nested_formula_depth = 4;

// Advance a shared formula-evaluation stack. CALCSTOCKINDEX and same-security
// indicator references use one depth/cycle policy so nesting cannot bypass a
// limit by alternating between the two mechanisms.
Json next_nested_formula_state(const Json* state,
                               const std::string& identity,
                               std::string_view consumer);

// Materialize literal CALCSTOCKINDEX calls as timestamp-keyed context series.
// The caller supplies only binding names emitted by the source analyzer; this
// module owns security parsing, nested formula execution and native alignment.
void bind_calcstockindex_context(
    Json& context,
    const std::filesystem::path& root,
    const std::string& current_market,
    const std::string& current_code,
    const Json& caller_kline,
    const std::vector<std::string>& bindings,
    const Json& formula_library,
    int timeout_ms,
    const BlockData* block_data,
    const std::filesystem::path& jsn_root,
    const Json* nested_state);

}  // namespace tdx::formula_context_detail
