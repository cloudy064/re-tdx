#pragma once

#include "formula_function_dispatch_internal.hpp"

#include <array>
#include <string_view>

namespace tdx::formula_engine_detail {

using TdxIndicatorHandler = Series (*)(
    const std::string&, const std::vector<Series>&,
    const Environment&, std::size_t);

Series native_recursive_average(Series values, int period);

Series evaluate_tdx_chip_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
Series evaluate_tdx_signal_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
Series evaluate_tdx_trend_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);
Series evaluate_tdx_band_volume_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size);

struct TdxIndicatorDefinition {
    std::string_view name;
    std::string_view family;
    TdxIndicatorHandler handler;
};

inline constexpr std::array<TdxIndicatorDefinition, 18> tdx_indicator_catalog{{
    {"TDXSSRP", "chip-distribution", evaluate_tdx_chip_indicator},
    {"TDXPAV", "chip-distribution", evaluate_tdx_chip_indicator},
    {"TDXPAVE", "chip-distribution", evaluate_tdx_chip_indicator},
    {"TDXMCST", "chip-distribution", evaluate_tdx_chip_indicator},
    {"TDXXLPLBASE", "proprietary-signal", evaluate_tdx_signal_indicator},
    {"TDXZXNH", "proprietary-signal", evaluate_tdx_signal_indicator},
    {"TDXNDB", "proprietary-signal", evaluate_tdx_signal_indicator},
    {"TDXSC", "proprietary-signal", evaluate_tdx_signal_indicator},
    {"TDXMSI", "trend", evaluate_tdx_trend_indicator},
    {"TDXVTY", "trend", evaluate_tdx_trend_indicator},
    {"TDXSAR", "trend", evaluate_tdx_trend_indicator},
    {"TDXASI", "trend", evaluate_tdx_trend_indicator},
    {"TDXBB", "band-volume", evaluate_tdx_band_volume_indicator},
    {"TDXWIDTH", "band-volume", evaluate_tdx_band_volume_indicator},
    {"TDXBOLLM", "band-volume", evaluate_tdx_band_volume_indicator},
    {"TDXNVI", "band-volume", evaluate_tdx_band_volume_indicator},
    {"TDXPVI", "band-volume", evaluate_tdx_band_volume_indicator},
    {"TDXKDJ", "band-volume", evaluate_tdx_band_volume_indicator},
}};

constexpr bool unique_indicator_names() {
    for (std::size_t left = 0; left < tdx_indicator_catalog.size(); ++left)
        for (std::size_t right = left + 1; right < tdx_indicator_catalog.size(); ++right)
            if (tdx_indicator_catalog[left].name == tdx_indicator_catalog[right].name)
                return false;
    return true;
}

static_assert(unique_indicator_names(), "TDX indicator names must be unique");

}  // namespace tdx::formula_engine_detail
