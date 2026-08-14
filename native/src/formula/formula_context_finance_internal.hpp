#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace tdx::formula_context_detail {

struct SplitBinding {
    std::string name;
    int occurrence{};
    int type{};
};

void bind_capital_history_context(
    Json& context, const Json& kline_document, const Json& capital_document,
    double current_circulating_shares);

void bind_split_context(
    Json& context, const Json& kline_document, const Json& capital_document,
    const std::vector<SplitBinding>& bindings);

void bind_point_in_time_finance_context(
    Json& context, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const Json& kline_document, const std::set<int>& finance_bindings,
    const std::set<int>& finvalue_bindings, int timeout_ms);

}  // namespace tdx::formula_context_detail
