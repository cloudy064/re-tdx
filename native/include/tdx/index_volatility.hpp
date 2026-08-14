#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct IndexVolatilityQuery {
    std::string view{"catalog"};
    std::string query;
    std::string market;
    std::string code;
    std::string start_date;
    std::string end_date;
    int window_days{5};
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_index_volatility_catalog_rows(
    const Json& rows, int window_days, const BlockData& blocks = {});
Json normalize_index_volatility_history_rows(const Json& rows);

class IndexVolatilityService {
public:
    IndexVolatilityService(std::filesystem::path root, BlockData blocks = {});
    Json query(const IndexVolatilityQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_index_volatility(const std::vector<std::string>& args);

}  // namespace tdx
