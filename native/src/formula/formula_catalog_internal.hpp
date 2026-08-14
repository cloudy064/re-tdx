#pragma once

#include "tdx/json.hpp"

#include <string>
#include <string_view>

namespace tdx::formula_context_detail {

enum class FormulaSelectionPolicy {
    context_free_ohlcv,
    automatic_nested_context,
    same_security_reference,
};

struct FormulaCatalogSelection {
    const Json* definition{};
    std::string code;
    std::string name;
    std::string output_name;
    Json analysis;
};

// Resolve a technical formula and its one-based output slot from the recovered
// TCalc catalog.  All consumers share this validation so catalog lookup and
// output-order semantics cannot drift between INSORT/INSUM/CALCSTOCKINDEX.
FormulaCatalogSelection select_technical_formula(
    const Json& formula_library,
    const std::string& requested,
    int output,
    FormulaSelectionPolicy policy,
    std::string_view consumer);

// Resolve the same catalog by its named output. TDX double-quoted references
// use `INDICATOR.OUTPUT`, so callers must not guess a one-based slot.
FormulaCatalogSelection select_technical_formula_output(
    const Json& formula_library,
    const std::string& requested,
    const std::string& output_name,
    FormulaSelectionPolicy policy,
    std::string_view consumer);

}  // namespace tdx::formula_context_detail
