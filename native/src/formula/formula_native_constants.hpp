#pragma once

namespace tdx::formula_engine_detail::tcalc_constants {

// Exact constants recovered from the native TCalc handlers. Keep their types
// explicit: several handlers depend on the float32 landing point while others
// retain the x87/double value through the comparison.
inline constexpr double absolute_epsilon = 0.000009999999747378752;
inline constexpr float absolute_epsilon_f =
    static_cast<float>(absolute_epsilon);
inline constexpr double relative_epsilon = 0.0000001000000011686097;
inline constexpr double integer_bias = 0.503000020980835;
inline constexpr long double integer_bias_l = 0.503000020980835L;

}  // namespace tdx::formula_engine_detail::tcalc_constants
