#pragma once

#include "formula_engine_internal.hpp"

#include <cstdint>
#include <optional>

namespace tdx::formula_engine_detail {

struct SarSeries {
    Series value;
    Series turn;
};

std::uint32_t tcalc_msvc_rand(std::uint32_t& state);
SarSeries parabolic_sar(
    const std::vector<Series>& args,
    const Environment& env,
    std::size_t size);
Series native_fftrans(const std::vector<Series>& args, std::size_t size);
Series native_newsar(
    const std::vector<Series>& args,
    const Environment& env,
    std::size_t size);
Series tcalc_sum(
    const Series& input,
    const Series& periods);
Series tcalc_highest_value(
    const Series& input,
    const Series& periods);
Series tcalc_lowest_value(
    const Series& input,
    const Series& periods);
Series tcalc_moving_average(
    const Series& input,
    const Series& periods);
Series tcalc_exponential_moving_average(
    const Series& input,
    const Series& periods);
Series tcalc_smoothed_moving_average(
    const Series& input,
    const Series& periods,
    const Series& weights);
enum class TcalcSeededExponentialKind {
    mema,
    expmema,
};
Series tcalc_seeded_exponential_moving_average(
    const Series& input,
    const Series& periods,
    TcalcSeededExponentialKind kind);
Series tcalc_count(
    const Series& input,
    const Series& periods);
Series tcalc_every(
    const Series& input,
    const Series& periods);
Series tcalc_exist(
    const Series& input,
    const Series& periods);
Series tcalc_bars_last(const Series& input);
Series tcalc_bars_last_count(const Series& input);
Series tcalc_highest_value_bars(
    const Series& input,
    const Series& periods);
Series tcalc_lowest_value_bars(
    const Series& input,
    const Series& periods);
Series tcalc_filter(
    const Series& input,
    const Series& periods);
Series tcalc_filterx(
    const Series& input,
    const Series& periods);
using FormulaFunctionStrategy = std::optional<Series> (*)(
    const std::string&,
    const std::vector<Series>&,
    const Environment&,
    std::size_t);

std::optional<Series> evaluate_host_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
std::optional<Series> evaluate_scalar_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
// TCalc gives direct numeric constants a type-3 fast path for a small set of
// scalar handlers.  Keep that AST distinction out of the generic Series API.
std::optional<Series> evaluate_literal_scalar_builtin_function(
    const std::string& name, double value, std::size_t size);
std::optional<Series> evaluate_reference_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
std::optional<Series> evaluate_future_shape_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
std::optional<Series> evaluate_core_series_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

std::optional<Series> evaluate_chip_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

std::optional<Series> evaluate_calendar_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

std::optional<Series> evaluate_context_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

std::optional<Series> evaluate_presentation_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

std::optional<Series> evaluate_series_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

std::optional<Series> evaluate_tdx_indicator_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

}  // namespace tdx::formula_engine_detail
