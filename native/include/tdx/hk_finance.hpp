#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct HkFinanceQuery {
    std::string code;
    std::string query;
    std::string classification;
    std::string report_from;
    std::string report_to;
    std::string sort{"code"};
    std::string order{"asc"};
    int offset{};
    int limit{200};
};

Json load_local_hk_finance(const std::filesystem::path& root,
                           const HkFinanceQuery& query);

int command_market_hk_finance(const std::vector<std::string>& args);

}  // namespace tdx
