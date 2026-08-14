#include "formula_operator_semantics_internal.hpp"
#include "formula_native_constants.hpp"

#include <cmath>
#include <limits>

namespace tdx::formula_operator_semantics_detail {
namespace {

constexpr double missing = std::numeric_limits<double>::quiet_NaN();
constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

bool narrow(double value, float& output) {
    if (!std::isfinite(value)) return false;
    output = static_cast<float>(value);
    return std::isfinite(output);
}

}  // namespace

double tcalc_divide(double numerator, double denominator,
                    double previous_result) {
    float native_numerator = 0.0F;
    float native_denominator = 0.0F;
    // TCalc leaves this bar missing when either input carries its missing
    // sentinel.  The near-zero carry applies only after both inputs validate.
    if (!narrow(numerator, native_numerator) ||
        !narrow(denominator, native_denominator))
        return missing;
    if (native_denominator < native_epsilon &&
        native_denominator > -native_epsilon)
        return previous_result;
    return static_cast<double>(
        static_cast<float>(native_numerator / native_denominator));
}

double tcalc_add(double left, double right) {
    float native_left = 0.0F;
    float native_right = 0.0F;
    // TCalc!sub_10003E50 reads both operands as dword floats, propagates its
    // canonical missing sentinel, and stores the sum back through dword.
    if (!narrow(left, native_left) || !narrow(right, native_right))
        return missing;
    const float result = native_left + native_right;
    return std::isfinite(result) ? static_cast<double>(result) : missing;
}

double tcalc_subtract(double left, double right) {
    float native_left = 0.0F;
    float native_right = 0.0F;
    if (!narrow(left, native_left) || !narrow(right, native_right))
        return missing;
    const float result = native_left - native_right;
    return std::isfinite(result) ? static_cast<double>(result) : missing;
}

double tcalc_multiply(double left, double right) {
    float native_left = 0.0F;
    float native_right = 0.0F;
    if (!narrow(left, native_left) || !narrow(right, native_right))
        return missing;
    const float result = native_left * native_right;
    return std::isfinite(result) ? static_cast<double>(result) : missing;
}

double tcalc_negate(double value) {
    // TCalc's compiler lowers unary minus to the ordinary raw-f32 multiply
    // primitive with a -1.0F constant.  Reuse that exact input/result landing
    // and missing propagation instead of negating the interpreter double.
    return tcalc_multiply(-1.0, value);
}

double tcalc_equal(EqualityOperator operation, double left, double right) {
    // TCalc!sub_100042A0/sub_100043E0 do not gate the canonical sentinel in
    // their per-bar path.  Restore interpreter missing to that finite raw-f32
    // value so sentinel/sentinel compares equal and sentinel/value does not.
    constexpr float native_missing_sentinel = -4.0398103e34F;
    const auto operand = [=](double value) {
        if (!std::isfinite(value)) return native_missing_sentinel;
        const float narrowed = static_cast<float>(value);
        return std::isfinite(narrowed) ? narrowed : native_missing_sentinel;
    };
    const double difference = static_cast<double>(operand(left)) -
                              static_cast<double>(operand(right));
    const bool equal = difference < native_epsilon &&
                       difference > -native_epsilon;
    return operation == EqualityOperator::equal
        ? (equal ? 1.0 : 0.0)
        : (equal ? 0.0 : 1.0);
}

double tcalc_compare(RelationalOperator operation, double left, double right) {
    float native_left = 0.0F;
    float native_right = 0.0F;
    if (!narrow(left, native_left) || !narrow(right, native_right))
        return missing;

    const double lhs = native_left;
    const double rhs = native_right;
    const double tolerance = std::abs(lhs) * relative_epsilon + native_epsilon;
    bool result = false;
    switch (operation) {
    case RelationalOperator::less:
        result = rhs >= lhs + tolerance;
        break;
    case RelationalOperator::less_equal:
        result = rhs > lhs - tolerance;
        break;
    case RelationalOperator::greater:
        result = rhs <= lhs - tolerance;
        break;
    case RelationalOperator::greater_equal:
        result = rhs < lhs + tolerance;
        break;
    }
    return result ? 1.0 : 0.0;
}

std::vector<double> tcalc_logical_not(const std::vector<double>& input) {
    std::vector<double> output(input.size(), missing);
    bool started = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const double value = input[index];
        // TCalc!sub_1000B590 scans past only the leading missing sentinel.
        // Once evaluation has started, a later sentinel follows the ordinary
        // non-zero branch and therefore produces zero.
        if (!started && !std::isfinite(value)) continue;
        started = true;
        output[index] = std::isfinite(value) &&
                                static_cast<float>(value) == 0.0F
                            ? 1.0
                            : 0.0;
    }
    return output;
}

}  // namespace tdx::formula_operator_semantics_detail
