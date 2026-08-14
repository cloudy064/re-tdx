#pragma once

#include "formula_engine_internal.hpp"
#include "formula_language_internal.hpp"
#include "formula_runtime_internal.hpp"

#include "tdx/json.hpp"

#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx::formula_runtime_detail {

struct FormulaEnvironment {
    formula_engine_detail::Environment numeric;
    StringEnvironment strings;
    Json parameters;
    Json context_bindings;
    bool uses_random{};
    std::uint32_t random_seed{};
    std::string random_seed_mode;
};

// Outcome of the RAND seed negotiation: whether the program calls RAND at all,
// and which seed the run was pinned to.
struct FormulaRandomSeed {
    bool uses_random{};
    std::uint32_t seed{};
    std::string mode;
};

// Builds the symbol table a parsed formula executes against.  `build()` runs the
// bind_* phases in a fixed order, which is load-bearing in three places:
// bind_external_indicators evaluates against the price fields and must not see
// later bindings, bind_context_turnover reads whichever earlier phase bound
// CAPITAL, and every context_bindings name is appended in phase order.
// Placement within `numeric` itself does not matter -- Environment is a map.
class FormulaEnvironmentBuilder {
public:
    FormulaEnvironmentBuilder(
        const Json& kline_document,
        const formula_language_detail::Program& program,
        const std::vector<Bar>& bars,
        const std::map<std::string, double>& parameters,
        const Json* context);

    FormulaEnvironment build() const;

private:
    // Rejects programs whose symbols need expansion-market columns that the
    // supplied K-lines do not carry.
    void verify_expansion_requirements() const;

    // Bar columns, their TdxW aliases, and the per-security constants.
    void bind_price_fields(formula_engine_detail::Environment& env) const;
    void bind_adjustment_flag(formula_engine_detail::Environment& env) const;
    void bind_directional_bars(formula_engine_detail::Environment& env) const;
    void bind_security_identity(formula_engine_detail::Environment& env,
                                StringEnvironment& string_env) const;
    void bind_external_indicators(formula_engine_detail::Environment& env) const;

    // Caller-supplied parameters; returns the echo of what was bound.
    Json bind_parameters(formula_engine_detail::Environment& env) const;

    // Optional evaluation context.  Returns the ordered binding-name list that
    // callers surface as `context_bindings`.
    Json bind_context(formula_engine_detail::Environment& env,
                      StringEnvironment& string_env) const;
    void bind_context_text_symbols(StringEnvironment& string_env) const;
    void bind_context_external_signals(formula_engine_detail::Environment& env,
                                        StringEnvironment& string_env,
                                        Json& names) const;
    void bind_context_catalog(StringEnvironment& string_env) const;
    void bind_context_numeric_groups(formula_engine_detail::Environment& env,
                                      Json& names) const;
    void bind_context_symbol_series(formula_engine_detail::Environment& env,
                                    Json& names) const;
    void bind_context_turnover(formula_engine_detail::Environment& env,
                                Json& names) const;
    void bind_context_ivolat(formula_engine_detail::Environment& env,
                              Json& names) const;

    // Series derived from bar position and bar timestamps.
    void bind_bar_position_series(formula_engine_detail::Environment& env) const;
    void bind_calendar_series(formula_engine_detail::Environment& env,
                              std::time_t now) const;
    void bind_intraday_series(formula_engine_detail::Environment& env) const;

    // Host/session constants, then the two series TdxW precomputes.
    void bind_session_constants(formula_engine_detail::Environment& env) const;
    void bind_derived_series(formula_engine_detail::Environment& env) const;

    FormulaRandomSeed resolve_random_seed(
        formula_engine_detail::Environment& env, std::time_t now) const;

    const Json& kline_document_;
    const formula_language_detail::Program& program_;
    const std::vector<Bar>& bars_;
    const std::map<std::string, double>& parameters_;
    const Json* context_;
};

}  // namespace tdx::formula_runtime_detail
