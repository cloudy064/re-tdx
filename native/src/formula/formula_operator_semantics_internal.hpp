#pragma once

#include <vector>

namespace tdx::formula_operator_semantics_detail {

enum class RelationalOperator {
    less,
    less_equal,
    greater,
    greater_equal,
};

enum class EqualityOperator {
    equal,
    not_equal,
};

double tcalc_divide(double numerator, double denominator,
                    double previous_result);
double tcalc_add(double left, double right);
double tcalc_subtract(double left, double right);
double tcalc_multiply(double left, double right);
double tcalc_negate(double value);
double tcalc_equal(EqualityOperator operation, double left, double right);
double tcalc_compare(RelationalOperator operation, double left, double right);
std::vector<double> tcalc_logical_not(const std::vector<double>& input);

}  // namespace tdx::formula_operator_semantics_detail
