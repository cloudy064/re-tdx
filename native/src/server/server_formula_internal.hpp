#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::server_detail {

struct RequestTarget {
    std::string path;
    std::map<std::string, std::string> query;
};

struct FormulaHttpState {
    const std::filesystem::path& root;
    const BlockData& block_data;
    const Json& formulas;
    const std::filesystem::path& jsn_root;
};

inline constexpr std::size_t maximum_formula_source_bytes = 16 * 1024;

std::string query_value(const RequestTarget& target, const std::string& key,
                        std::string fallback = {});
const Json* json_member(const Json& value, std::string_view key);
std::string json_body_string(const Json& body, std::string_view key,
                             std::string fallback = {});
std::string formula_upper_ascii(std::string value);
void merge_formula_request_target(RequestTarget& target, const Json& body);
void merge_formula_adjustment_target(RequestTarget& target, const Json& body);
int parse_bounded(const std::string& text, std::string_view name,
                  int minimum, int maximum);
std::pair<std::string, std::string> query_security(
    const RequestTarget& target);
std::pair<std::string, std::string> query_kline_security(
    const RequestTarget& target);
bool query_bool(const RequestTarget& target, const std::string& key,
                bool fallback = false);
void attach_security_metadata(const BlockData& block_data, Json& document,
                              const std::string& market,
                              const std::string& code);
Json apply_requested_kline_adjustment(
    const std::filesystem::path& root, const RequestTarget& target,
    const std::string& market, const std::string& code,
    const std::string& kind, int timeout, Json document);

Json query_formulas(const FormulaHttpState& state,
                    const RequestTarget& target);
Json query_formula_calculation(const RequestTarget& target);
Json query_formula_coverage(const FormulaHttpState& state,
                            const RequestTarget& target);
Json query_formula_context_template(const FormulaHttpState& state,
                                    const RequestTarget& target);
Json query_formula_context_import(const Json& body);
Json formula_audit_context_requirements(const Json& formulas,
                                        bool expansion_market,
                                        std::string_view market);
Json query_formula_audit(const FormulaHttpState& state,
                         const RequestTarget& target);
Json query_cloud_calc_audit(const FormulaHttpState& state,
                            const RequestTarget& target);
Json query_cloud_calc_template(const FormulaHttpState& state,
                               const RequestTarget& target);
Json query_cloud_calc_execution(const FormulaHttpState& state,
                                const Json& body);
Json query_cloud_calc_batch_execution(const FormulaHttpState& state,
                                      const Json& body);
Json query_inline_tpool_evaluation(const FormulaHttpState& state,
                                   const Json& body);
Json query_formula_execution(const FormulaHttpState& state,
                             const RequestTarget& target);
Json query_post_formula_execution(const FormulaHttpState& state,
                                  const RequestTarget& target,
                                  const Json& body);
Json query_formula_backtest(const FormulaHttpState& state,
                            const RequestTarget& target,
                            const Json& formulas);
Json query_inline_formula_backtest(const FormulaHttpState& state,
                                   const RequestTarget& target,
                                   const Json& body);
Json query_formula_scan(const FormulaHttpState& state,
                        const RequestTarget& target);
Json query_inline_formula_scan(const FormulaHttpState& state,
                               const RequestTarget& target,
                               const Json& body);
Json query_formula_strategy(const FormulaHttpState& state,
                            const RequestTarget& target,
                            const Json& body,
                            bool historical_backtest);

}  // namespace tdx::server_detail
