#include "formula_tdx_indicators_internal.hpp"

namespace tdx::formula_engine_detail {

std::optional<Series> evaluate_tdx_indicator_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    for (const auto& definition : tdx_indicator_catalog)
        if (definition.name == name)
            return definition.handler(name, args, env, size);
    return std::nullopt;
}

}  // namespace tdx::formula_engine_detail

