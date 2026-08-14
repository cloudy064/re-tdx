#pragma once

#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::formula_engine_detail {

using Series = std::vector<double>;
using Environment = std::map<std::string, Series, std::less<>>;
inline constexpr double missing = std::numeric_limits<double>::quiet_NaN();

struct DirectionalBarSeries {
    Series open;
    Series high;
    Series low;
    Series close;
    Series volume;
};

extern const std::set<std::string> supported_functions;
extern const std::set<std::string> custom_formula_core_functions;
extern const std::set<std::string> custom_formula_transform_functions;
extern const std::set<std::string> custom_formula_future_path_functions;
extern const std::set<std::string> custom_formula_random_functions;
extern const std::set<std::string> custom_formula_directional_bar_functions;
extern const std::set<std::string> custom_formula_adjustment_functions;
extern const std::set<std::string> custom_formula_host_calendar_functions;
extern const std::set<std::string> custom_formula_capital_turnover_functions;
extern const std::set<std::string> custom_formula_machine_clock_functions;
extern const std::set<std::string> custom_formula_core_symbols;
extern const std::set<std::string> custom_formula_sequence_statistics_functions;
extern const std::set<std::string> custom_formula_rolling_variance_functions;
extern const std::set<std::string> custom_formula_benchmark_cumulative_functions;
extern const std::set<std::string> custom_formula_calendar_filter_functions;
extern const std::set<std::string> custom_formula_calendar_filter_symbols;
extern const std::set<std::string> custom_formula_security_string_functions;
extern const std::set<std::string> custom_formula_security_string_symbols;
extern const std::set<std::string> custom_formula_contract_metadata_symbols;
extern const std::set<std::string> custom_formula_security_stat_functions;
extern const std::set<std::string> custom_formula_industry_valuation_functions;
extern const std::set<std::string> custom_formula_market_breadth_functions;
extern const std::set<std::string> custom_formula_dynamic_quote_functions;
extern const std::set<std::string> custom_formula_security_relation_functions;
extern const std::set<std::string> custom_formula_security_relation_text_symbols;
extern const std::set<std::string> custom_formula_divfactor_functions;
extern const std::set<std::string> custom_formula_kline_auxiliary_symbols;
extern const std::set<std::string> custom_formula_block_metadata_functions;
extern const std::set<std::string> custom_formula_indicator_aggregate_functions;
extern const std::set<std::string> custom_formula_block_metadata_symbols;
extern const std::set<std::string> custom_formula_single_point_functions;
extern const std::set<std::string> custom_formula_external_signal_functions;
extern const std::set<std::string> custom_formula_external_series_functions;
extern const std::set<std::string> custom_formula_type167_text_symbols;
extern const std::set<std::string> custom_formula_security_score_functions;
extern const std::set<std::string> tcalc_registry_syntax_only_names;
extern const std::set<std::string> tcalc_registry_broker_private_signal_names;
extern const std::set<std::string> tcalc_registry_level2_order_flow_names;
extern const std::set<std::string> tcalc_registry_live_trading_state_names;
extern const std::set<std::string> tcalc_registry_live_trading_context_names;
extern const std::set<std::string> tcalc_registry_plugin_callback_names;
extern const std::set<std::string> presentation_only_functions;
extern const std::set<std::string> string_surrogate_functions;
extern const std::set<std::string> numeric_surrogate_functions;
extern const std::set<std::string> presentation_degraded_functions;
extern const std::set<std::string> render_ir_functions;
extern const std::set<std::string> future_functions;
extern const std::set<std::string> external_functions;
extern const std::set<std::string> market_symbols;
extern const std::set<std::string> constants;
extern const std::set<std::string> string_symbols;
extern const std::set<std::string> intrinsic_formula_symbols;
extern const std::set<std::string> builtin_symbols;
extern const std::set<std::string> external_symbols;
extern const std::set<std::string> explicit_context_symbols;
extern const std::set<std::string> context_external_dependencies;
extern const std::set<int> finance_context_ids;
extern const std::set<int> dynainfo_context_ids;
extern const std::set<std::string> render_event_functions;

Series constant(double value, std::size_t size);
bool truth(double value);
// TCalc IF/IFF skip only the leading missing region.  Once evaluation has
// started, the native handler selects the true branch for every exact
// non-zero value, including the native missing sentinel.
bool tcalc_if_select_true(double value);
int tcalc_external_integer(double value);
std::string tcalc_external_binding_key(double namespace_value,
                                       double external_id);
double native_float(double value);
std::string tcalc_number_text(double value, double decimals);
std::size_t tcalc_string_byte_length(std::string_view value);
std::string tcalc_substring(std::string_view value, double position,
                            double length);
long long calendar_day_number(int year, unsigned month, unsigned day);
int tcalc_market_id(std::string market);
bool tcalc_lfs_security_supported(const std::string& market,
                                  const std::string& code);
double tcalc_mcst_volume_scale(const std::string& market,
                              const std::string& code);
bool tcalc_zxnh_uses_typical_price(const std::string& market,
                                   const std::string& code);
int period_at(const Series& value, std::size_t index, int minimum = 0);
void require_arity(const std::string& name, const std::vector<Series>& args,
                   std::size_t minimum, std::size_t maximum);
DirectionalBarSeries tcalc_directional_bars(const Environment& env,
                                             std::size_t size);
Series evaluate_call(const std::string& name, const std::vector<Series>& args,
                     const Environment& env, std::size_t size);

}  // namespace tdx::formula_engine_detail
