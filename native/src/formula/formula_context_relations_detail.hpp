#pragma once

#include "formula_context_relations_internal.hpp"

#include "tdx/options.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace tdx::formula_context_detail {

const Json *optional(const Json &object, std::string_view key);
std::string text_or(const Json &object, std::string_view key,
                    std::string fallback = {});
int integer_or(const Json &object, std::string_view key, int fallback);
bool starts_with(std::string_view value, std::string_view prefix);

bool needs_industry_context(const std::set<std::string> &dependencies);
bool needs_concept_text_context(const std::set<std::string> &dependencies);
bool needs_block_metadata_context(const std::set<std::string> &dependencies);
bool needs_block_code_function_context(
    const std::set<std::string> &dependencies);

struct ExternalSecurityBinding {
    std::string name;
    std::string market;
    std::string code;
    std::string field;
};

Json external_security_kline(const Json &target,
                             const ExternalSecurityBinding &binding,
                             int timeout_ms);
std::optional<OptionInstrument> resolve_formula_option_instrument(
    const Json &target, const std::string &market, const std::string &code,
    int timeout_ms);
void bind_ivolat_context(Json &context, const std::filesystem::path &root,
                         const Json &target, const std::string &market,
                         const std::string &code,
                         const std::set<std::string> &dependencies,
                         int timeout_ms);
void bind_beta_series(Json &context, const Json &target,
                      const std::string &market, const std::string &code,
                      const std::set<std::string> &dependencies,
                      int timeout_ms);
void bind_index_series(Json &context, const Json &target,
                       const std::string &market, const std::string &code,
                       const std::set<std::string> &dependencies,
                       int timeout_ms);
void bind_external_security_series(
    Json &context, const Json &target,
    const std::set<std::string> &dependencies, int timeout_ms);

int market_id_for(std::string market);
std::string tdx_region_name(int province_id);
std::string concept_block_text(const BlockData &data,
                               const std::string &market,
                               const std::string &code);
void bind_formula_main_index_text(
    Json &context, Json &symbols, const std::string &market,
    const std::string &code, const std::set<std::string> &dependencies);
void bind_formula_underlying(
    Json &context, Json &symbols, const std::filesystem::path &root,
    const Json *target, const std::string &market, const std::string &code,
    const std::set<std::string> &dependencies, int timeout_ms);

const Block *industry_block_for(const BlockData &data, std::string market,
                                const std::string &code);
void bind_industry_valuation_functions(
    Json &context, const std::filesystem::path &root,
    const std::filesystem::path &jsn_root, const std::string &market,
    const std::string &code, const std::set<std::string> &dependencies,
    const BlockData &block_data, int security_type_value);
void bind_industry_series(
    Json &context, const std::filesystem::path &root, const Json &target,
    const std::string &market, const std::string &code,
    const std::set<std::string> &dependencies, int timeout_ms,
    const BlockData &block_data);

} // namespace tdx::formula_context_detail
