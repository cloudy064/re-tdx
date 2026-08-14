#pragma once

#include "formula_context_professional_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>

namespace tdx::formula_context_detail {

struct RelationBlockRequirements {
    bool industry{};
    bool concept_text{};
    bool metadata{};
    bool code_functions{};
    std::set<std::string> families;

    bool any() const {
        return industry || concept_text || metadata || code_functions;
    }
};

RelationBlockRequirements relation_block_requirements(
    const std::set<std::string>& dependencies);

void bind_static_security_relations(
    Json& context, Json& symbols,
    const std::filesystem::path& root,
    const std::filesystem::path& jsn_root,
    const std::string& market, const std::string& code,
    const Json* kline_document,
    const std::set<std::string>& dependencies,
    const BlockData* block_data,
    int security_type,
    int timeout_ms);

void bind_security_relation_series(
    Json& context,
    const std::filesystem::path& root,
    const Json& kline_document,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies,
    const BlockData* block_data,
    int timeout_ms);

void bind_region_text(Json& context, Json& symbols, int province_id);
void bind_concept_text(Json& context, Json& symbols,
                       const BlockData& block_data,
                       const std::string& market,
                       const std::string& code);

ProfessionalBoardTarget resolve_professional_board_target(
    const std::string& market, const std::string& code,
    int security_type, const BlockData* block_data);

}  // namespace tdx::formula_context_detail
