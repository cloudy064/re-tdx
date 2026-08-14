#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct HistoricalSecurityName {
    int market_id{};
    std::string code;
    std::string name;
    std::size_t source_line{};
};

using HistoricalSecurityNameCatalog =
    std::map<std::pair<int, std::string>, HistoricalSecurityName>;

struct HistoricalSecurityQuery {
    std::string market{"all"};
    std::string code;
    std::string query;
    std::string presence{"all"};
    std::string sort{"code"};
    std::string order{"asc"};
    int offset{0};
    int limit{1000};
};

HistoricalSecurityNameCatalog load_local_historical_security_names(
    const std::filesystem::path& root);

Json load_local_historical_securities(
    const std::filesystem::path& root,
    const HistoricalSecurityQuery& query,
    const SecurityCatalog& current_securities = {});

int command_market_historical_securities(const std::vector<std::string>& args);

}  // namespace tdx
