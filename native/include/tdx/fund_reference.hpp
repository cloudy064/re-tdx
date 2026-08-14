#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct FundReferenceQuery {
    std::string view{"all"};
    std::string market;
    std::string code;
    std::string query;
    std::string as_of_date;
    int limit{5000};
};

Json load_local_fund_reference(
    const std::filesystem::path& root,
    const FundReferenceQuery& query,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

int command_market_fund_reference(const std::vector<std::string>& args);

}  // namespace tdx
