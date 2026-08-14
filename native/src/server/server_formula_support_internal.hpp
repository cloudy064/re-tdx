#pragma once

#include "server_formula_internal.hpp"

#include "tdx/json.hpp"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::server_detail {

struct FormulaKlineFetchOutcome {
    Json kline;
    std::string error;
};

struct InlineFormulaRequest {
    std::string source;
    std::string code;
    std::map<std::string, double> parameters;
    Json definition;
    Json analysis;
};

std::string requested_kline_adjustment_mode(const RequestTarget &target);
void attach_multi_security_adjustment_summary(
    Json &result, const std::vector<Json> &klines, const std::string &mode);
std::vector<FormulaKlineFetchOutcome> fetch_adjusted_formula_klines(
    const FormulaHttpState &state, const RequestTarget &target,
    const std::vector<std::pair<std::string, std::string>> &securities,
    const std::string &period, int pages, int page_size, int timeout,
    int workers);
double parse_formula_parameter(const std::string &text, std::string_view name);
bool formula_dependency_supported_in_expansion(std::string_view name,
                                               std::string_view market = {});
InlineFormulaRequest parse_inline_formula_request(
    const Json &body, const std::string &formula_kind);

} // namespace tdx::server_detail
