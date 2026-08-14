#include "formula_context_relations_detail.hpp"
#include "formula_context_relations_catalog.hpp"

#include "formula_context_support_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace tdx::formula_context_detail {

RelationBlockRequirements relation_block_requirements(
    const std::set<std::string>& dependencies) {
    RelationBlockRequirements result;
    result.industry = needs_industry_context(dependencies);
    result.concept_text = needs_concept_text_context(dependencies);
    result.metadata = needs_block_metadata_context(dependencies);
    result.code_functions = needs_block_code_function_context(dependencies);
    if (result.industry) {
        result.families.insert("industry");
        result.families.insert("research-industry");
    }
    if (result.concept_text) result.families.insert("concept");
    if (result.metadata) {
        result.families.insert("industry");
        result.families.insert("research-industry");
        result.families.insert("concept");
        result.families.insert("style");
        result.families.insert("index");
    }
    if (dependencies.count("GNBKZSCODE")) result.families.insert("concept");
    if (dependencies.count("FGBKZSCODE")) result.families.insert("style");
    return result;
}

void bind_static_security_relations(
    Json& context, Json& symbols,
    const std::filesystem::path& root,
    const std::filesystem::path& jsn_root,
    const std::string& market, const std::string& code,
    const Json* kline_document,
    const std::set<std::string>& dependencies,
    const BlockData* block_data,
    int security_type_value,
    int timeout_ms) {
    bind_formula_main_index_text(context, symbols, market, code, dependencies);
    bind_formula_underlying(context, symbols, root, kline_document, market,
                            code, dependencies, timeout_ms);
    if (dependencies.count("HYSYL") || dependencies.count("HYSJL")) {
        if (!block_data)
            throw Error("formula industry valuation context requires block data");
        bind_industry_valuation_functions(
            context, root, jsn_root, market, code, dependencies,
            *block_data, security_type_value);
    }
}

void bind_security_relation_series(
    Json& context, const std::filesystem::path& root,
    const Json& kline_document,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies,
    const BlockData* block_data,
    int timeout_ms) {
    bind_ivolat_context(context, root, kline_document, market, code,
                        dependencies, timeout_ms);
    bind_beta_series(context, kline_document, market, code, dependencies,
                     timeout_ms);
    bind_index_series(context, kline_document, market, code, dependencies,
                      timeout_ms);
    bind_external_security_series(context, kline_document, dependencies,
                                  timeout_ms);
    if (needs_industry_context(dependencies)) {
        if (!block_data)
            throw Error("formula industry context requires block data");
        bind_industry_series(context, root, kline_document, market, code,
                             dependencies, timeout_ms, *block_data);
    }
}

void bind_region_text(Json& context, Json& symbols, int province_id) {
    bind_formula_text_symbol(
        context, symbols, "DYBLOCK", tdx_region_name(province_id),
        "tdxw-type120-offset155-exact-province-table");
}

void bind_concept_text(Json& context, Json& symbols,
                       const BlockData& block_data,
                       const std::string& market,
                       const std::string& code) {
    bind_formula_text_symbol(
        context, symbols, "GNBLOCK",
        concept_block_text(block_data, market, code),
        "tdx-command8-category1-infoharbor-concept-membership");
}
ProfessionalBoardTarget resolve_professional_board_target(
    const std::string& market, const std::string& code,
    int security_type_value, const BlockData* block_data) {
    std::string target_market = normalized_market(market);
    std::string mode;
    const bool index = security_type_value == 0;
    if (!index && !(target_market == "sh" &&
                    (starts_with(code, "880") || starts_with(code, "881")))) {
        if (!block_data)
            throw Error("BKJYONE requires local industry block data");
        const auto* block = industry_block_for(*block_data, market, code);
        if (!block || block->block_code.empty())
            throw Error("BKJYONE found no direct industry assignment");
        target_market = "sh";
        mode = "tdxw-type120-current-security-direct-industry";
        return {std::move(target_market), block->block_code, std::move(mode)};
    }

    std::uint32_t numeric = 0;
    try { numeric = static_cast<std::uint32_t>(std::stoul(code)); }
    catch (...) {
        return {std::move(target_market), code,
                "tdxw-type175-original-index-security"};
    }
    const auto mapped = std::find_if(
        broad_index_bindings.begin(), broad_index_bindings.end(),
        [numeric](const BroadIndexBinding& binding) {
            return binding.source == numeric;
        });
    if (mapped == broad_index_bindings.end()) {
        mode = "tdxw-type175-original-index-security";
        return {std::move(target_market), code, std::move(mode)};
    }
    target_market = "sh";
    mode = "tdxw-sub-4D19B0-broad-index-map";
    return {std::move(target_market), std::string(mapped->industry_index),
            std::move(mode)};
}


} // namespace tdx::formula_context_detail
