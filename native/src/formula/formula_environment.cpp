#include "formula_environment_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <utility>

namespace tdx::formula_runtime_detail {
using namespace formula_engine_detail;
using namespace formula_language_detail;

FormulaEnvironmentBuilder::FormulaEnvironmentBuilder(
    const Json& kline_document,
    const Program& program,
    const std::vector<Bar>& bars,
    const std::map<std::string, double>& parameters,
    const Json* context)
    : kline_document_(kline_document),
      program_(program),
      bars_(bars),
      parameters_(parameters),
      context_(context) {}

void FormulaEnvironmentBuilder::verify_expansion_requirements() const {
    const bool needs_open_interest = program_.symbols.count("VOLINSTK") ||
                                     program_.symbols.count("CCL");
    const bool needs_hk_short_volume = program_.symbols.count("HKSHORTVOL");
    if (needs_open_interest &&
        !std::any_of(bars_.begin(), bars_.end(), [](const Bar& bar) {
            return std::isfinite(bar.open_interest);
        }))
        throw Error("formula requires expansion-market open_interest (VOLINSTK/CCL); "
                    "fetch K-lines from TDX 7727 first");
    if (needs_hk_short_volume &&
        !std::any_of(bars_.begin(), bars_.end(), [](const Bar& bar) {
            return std::isfinite(bar.hk_short_volume);
        }))
        throw Error("formula requires HK expansion-market hk_short_volume (HKSHORTVOL)");
}

FormulaEnvironment FormulaEnvironmentBuilder::build() const {
    verify_expansion_requirements();
    Environment env;
    StringEnvironment string_env;
    bind_price_fields(env);
    bind_adjustment_flag(env);
    bind_directional_bars(env);
    bind_security_identity(env, string_env);
    // Runs against the price fields bound above; it must not observe the
    // parameter, context or calendar bindings that follow.
    bind_external_indicators(env);
    env["TRUE"] = constant(1.0, bars_.size());
    env["FALSE"] = constant(0.0, bars_.size());
    env["DRAWNULL"] = constant(missing, bars_.size());
    auto actual = bind_parameters(env);
    // Must precede bind_context_turnover's CAPITAL lookup, which the context
    // phase performs after its own numeric bindings.
    auto context_names = bind_context(env, string_env);
    // One clock reading feeds both DAYSTOTODAY/MACHINE* and the RAND seed.
    const std::time_t now = std::time(nullptr);
    bind_bar_position_series(env);
    bind_calendar_series(env, now);
    bind_intraday_series(env);
    bind_session_constants(env);
    bind_derived_series(env);
    auto random = resolve_random_seed(env, now);
    FormulaEnvironment result;
    result.numeric = std::move(env);
    result.strings = std::move(string_env);
    result.parameters = std::move(actual);
    result.context_bindings = std::move(context_names);
    result.uses_random = random.uses_random;
    result.random_seed = random.seed;
    result.random_seed_mode = std::move(random.mode);
    return result;
}

}  // namespace tdx::formula_runtime_detail
