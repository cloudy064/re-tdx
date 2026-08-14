#pragma once

#include "tdx/json.hpp"

#include <array>
#include <string>
#include <string_view>

namespace tdx::recon_contract_detail {

using FormulaContractValidator = bool (*)(const std::string& contract_id,
                                          const Json& document,
                                          Json& result);

struct FormulaRenderContractStrategy {
    std::string_view contract_id;
    FormulaContractValidator validator;
};

bool validate_formula_price_annotations_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_sequence_annotation_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_series_sticks_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_native_series_styles_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_colorref_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_render_order_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_render_primitives_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_background_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);

inline constexpr std::array<FormulaRenderContractStrategy, 19>
    formula_render_contract_strategies{{
        {"formula-price-annotations-inline-post",
         validate_formula_price_annotations_contract},
        {"formula-drawnumber-dif-inline-post",
         validate_formula_sequence_annotation_contract},
        {"formula-series-sticks-inline-post",
         validate_formula_series_sticks_contract},
        {"formula-native-series-styles-inline-post",
         validate_formula_native_series_styles_contract},
        {"formula-colorref-inline-post", validate_formula_colorref_contract},
        {"formula-mixed-render-order-inline-post",
         validate_formula_render_order_contract},
        {"formula-sqjz-sequence-live",
         validate_formula_render_primitives_contract},
        {"formula-wavekx-partline-live",
         validate_formula_render_primitives_contract},
        {"formula-rgband-fill-live",
         validate_formula_render_primitives_contract},
        {"formula-tjcjl-stickline-live",
         validate_formula_render_primitives_contract},
        {"formula-ichimoku-stickline-live",
         validate_formula_render_primitives_contract},
        {"formula-cyx-drawline-live",
         validate_formula_render_primitives_contract},
        {"formula-fkx-candles-live",
         validate_formula_render_primitives_contract},
        {"formula-slzt-linestick-live",
         validate_formula_render_primitives_contract},
        {"formula-cpbs-text-live",
         validate_formula_render_primitives_contract},
        {"formula-fscage-number-live",
         validate_formula_render_primitives_contract},
        {"formula-drawcframe-inline-post",
         validate_formula_render_primitives_contract},
        {"formula-drawicon-inline-post",
         validate_formula_render_primitives_contract},
        {"formula-background-inline-post",
         validate_formula_background_contract},
    }};

bool validate_formula_foundation_contract(const std::string& contract_id,
                                          const Json& document,
                                          Json& result);
bool validate_formula_calculation_contract(const std::string& contract_id,
                                           const Json& document,
                                           Json& result);
bool validate_formula_render_contract(const std::string& contract_id,
                                      const Json& document,
                                      Json& result);
bool validate_formula_runtime_contract(const std::string& contract_id,
                                       const Json& document,
                                       Json& result);
bool validate_formula_workflow_contract(const std::string& contract_id,
                                        const Json& document,
                                        Json& result);

}  // namespace tdx::recon_contract_detail
