#include "formula_engine_internal.hpp"
#include "formula_function_dispatch_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/options.hpp"
#include "tdx/security_status.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace tdx::formula_engine_detail {

namespace {

constexpr std::array<FormulaFunctionStrategy, 5> builtin_function_strategies{{
    evaluate_host_builtin_function,
    evaluate_scalar_builtin_function,
    evaluate_reference_builtin_function,
    evaluate_future_shape_builtin_function,
    evaluate_core_series_builtin_function,
}};

}  // namespace

Series evaluate_call(const std::string& name, const std::vector<Series>& args,
                     const Environment& env, std::size_t size) {
    for (const auto strategy : builtin_function_strategies)
        if (auto result = strategy(name, args, env, size))
            return std::move(*result);
    throw Error("unsupported formula function: " + name);
}
}  // namespace tdx::formula_engine_detail
