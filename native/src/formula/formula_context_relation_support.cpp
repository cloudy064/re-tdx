#include "formula_context_relations_detail.hpp"
#include "formula_context_relations_catalog.hpp"

#include <algorithm>

namespace tdx::formula_context_detail {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key,
                    std::string fallback) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string()
                                       : std::move(fallback);
}

int integer_or(const Json& object, std::string_view key, int fallback) {
    const auto* value = optional(object, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : fallback;
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}
bool needs_industry_context(const std::set<std::string>& dependencies) {
    return has_any_dependency(dependencies, industry_context_symbols);
}

bool needs_concept_text_context(const std::set<std::string>& dependencies) {
    return dependencies.count("GNBLOCK") != 0;
}

bool needs_block_metadata_context(const std::set<std::string>& dependencies) {
    return has_any_dependency(dependencies, block_metadata_symbols);
}

bool needs_block_code_function_context(
    const std::set<std::string>& dependencies) {
    return dependencies.count("GNBKZSCODE") ||
           dependencies.count("FGBKZSCODE") ||
           dependencies.count("GETNAMEOFCODE");
}


} // namespace tdx::formula_context_detail
