#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>

namespace tdx::formula_context_detail {

struct DynamicQuoteRequirements {
    std::set<int> selectors;
    std::set<std::string> aliases;
    bool buy_volume{};
    bool sell_volume{};

    bool requires_finance() const { return selectors.count(39) != 0; }
};

DynamicQuoteRequirements dynamic_quote_requirements(
    const Json& analysis,
    const std::set<std::string>& dependencies);

void bind_dynamic_quote_context(
    Json& context,
    Json& symbols,
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const Json* kline_document,
    const DynamicQuoteRequirements& requirements,
    const Json* finance_record,
    int security_type,
    int timeout_ms,
    const BlockData* block_data);

}  // namespace tdx::formula_context_detail
