#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct HotHistoryQuery {
    std::string market;
    std::string code;
    std::string query;
    std::string date_from;
    std::string date_to;
    std::string sort{"start-date"};
    std::string order{"desc"};
    int offset{0};
    int limit{500};
};

Json load_local_hot_history(
    const std::filesystem::path& root,
    const HotHistoryQuery& query,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

int command_market_hot_history(const std::vector<std::string>& args);

}  // namespace tdx
