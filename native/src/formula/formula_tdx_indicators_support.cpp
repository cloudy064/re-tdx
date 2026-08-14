#include "formula_tdx_indicators_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>

namespace tdx::formula_engine_detail {

Series native_recursive_average(Series values, int period) {
    const auto first = std::find_if(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
    if (first == values.end()) return values;
    const auto first_index = static_cast<std::size_t>(first - values.begin());
    if (period < 1 || first_index + static_cast<std::size_t>(period) > values.size())
        return values;
    for (std::size_t i = first_index + 1; i < values.size(); ++i) {
        if (!std::isfinite(values[i]) || !std::isfinite(values[i - 1])) continue;
        values[i] = native_float(
            (values[i - 1] * static_cast<double>(period - 1) + values[i]) /
            static_cast<double>(period));
    }
    return values;
}


}  // namespace tdx::formula_engine_detail

