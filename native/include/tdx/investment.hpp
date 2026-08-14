#pragma once

#include "tdx/json.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

struct InvestmentQuery {
    std::filesystem::path private_directory;
    std::filesystem::path fee_rules_path;
    std::filesystem::path quote_snapshot_path;
    std::string view{"summary"};
    std::string market;
    std::string code;
    std::string side;
    std::string portfolio_name;
    bool include_notes{};
    bool include_closed{};
    std::size_t offset{};
    std::size_t limit{1000};
    std::optional<double> price;
    std::optional<std::uint64_t> quantity;
    int timeout_ms{10000};
};

Json load_local_investment(const std::filesystem::path& root,
                           const InvestmentQuery& query);

int command_market_investment(const std::vector<std::string>& args);

}  // namespace tdx
