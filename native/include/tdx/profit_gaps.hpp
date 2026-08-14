#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct ProfitGapsQuery {
    std::string query;
    std::string market;
    std::string code;
    int minimum_safety{60};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_profit_gap_rows(const Json& rows, const BlockData& blocks = {});

class ProfitGapsService {
public:
    ProfitGapsService(std::filesystem::path root, BlockData blocks = {});
    Json query(const ProfitGapsQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_profit_gaps(const std::vector<std::string>& args);

}  // namespace tdx
