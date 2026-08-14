#include "recon_contract_formula_foundation_internal.hpp"

#include "recon_contract_internal.hpp"

namespace tdx::recon_contract_detail {

void validate_formula_coverage_contract(const Json& document, Json& result) {
    const bool identity =
        number_is(member(document, "analysis_schema_version"), 7.0) &&
        string_is(member(document, "formula_engine"), "tdx-source-interpreter-v1");
    add_assertion(result, "analysis_identity", identity,
                  "schema 7 / tdx-source-interpreter-v1", identity);
    const auto* total = member(document, "total");
    const bool valid_total = total && total->is_number() &&
        total->as_number() >= 379.0;
    const auto matches_total = [&](const char* name) {
        const auto* value = member(document, name);
        return valid_total && value && value->is_number() &&
               value->as_number() == total->as_number();
    };
    const auto* numeric_signal_safe = member(document, "numeric_signal_safe");
    const auto* degraded_numeric_output = member(document, "degraded_numeric_output");
    const bool numeric_complete = valid_total &&
        matches_total("source_available") &&
        matches_total("syntax_supported") &&
        numeric_signal_safe && numeric_signal_safe->is_number() &&
        degraded_numeric_output && degraded_numeric_output->is_number() &&
        degraded_numeric_output->as_number() == 0.0 &&
        numeric_signal_safe->as_number() == total->as_number();
    add_assertion(result, "numeric_fidelity", numeric_complete,
                  "all loaded formulas source+syntax complete and numeric-safe; ZTPRICE/DTPRICE require exact caller-owned raw host context; system baseline >=379",
                  numeric_complete);
    const auto* graphics = member(document, "graphics");
    const bool valid_graphics = graphics && graphics->is_number() &&
        graphics->as_number() >= 90.0;
    const bool render_complete = valid_graphics &&
        number_is(member(document, "render_ir_available"),
                  graphics->as_number()) &&
        number_is(member(document, "render_semantics_materialized"),
                  graphics->as_number()) &&
        matches_total("presentation_semantics_faithful") &&
        number_is(member(document, "unsupported_presentation_directive"), 0.0);
    add_assertion(result, "render_fidelity", render_complete,
                  "all loaded graphics have render IR; all loaded formulas presentation-faithful; system graphics baseline >=90",
                  render_complete);
    const bool surrogate_separated =
        number_is(member(document, "semantic_surrogate"), 56.0) &&
        number_is(member(document, "presentation_return_surrogate"), 56.0) &&
        number_is(member(document, "degraded_numeric_output"), 0.0);
    add_assertion(result, "presentation_return_separated", surrogate_separated,
                  "56 presentation-return placeholders remain separate from numeric fidelity; limit-price host context is explicit rather than approximated",
                  surrogate_separated);
    const auto* by_kind = member(document, "by_kind");
    const auto* technical = by_kind ? member(*by_kind, "technical") : nullptr;
    const auto* color_k = by_kind ? member(*by_kind, "color-k") : nullptr;
    const bool kind_partition = technical && color_k &&
        number_is(member(*technical, "graphics"), 88.0) &&
        number_is(member(*technical, "render_semantics_materialized"), 88.0) &&
        number_is(member(*color_k, "graphics"), 2.0) &&
        number_is(member(*color_k, "render_semantics_materialized"), 2.0);
    add_assertion(result, "render_kind_partition", kind_partition,
                  "technical 88 + color-k 2", kind_partition);
    const auto* capabilities = member(document, "capabilities");
    const bool custom_capabilities = capabilities &&
        validate_formula_capabilities(*capabilities);
    add_assertion(result, "custom_formula_capabilities", custom_capabilities,
                  "390 registry entries fully classified: 317 recognized, 73 runtime-boundary names; 262 functions / 87 automatic symbols",
                  custom_capabilities);
}

}  // namespace tdx::recon_contract_detail
