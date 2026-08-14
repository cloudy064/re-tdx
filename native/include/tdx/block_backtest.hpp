#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct BlockBacktestQuery {
    std::string category{"all"};
    std::string block_code;
    std::string begin_date;
    std::string end_date;
    std::string adjustment{"forward"};
    std::string sort{"return"};
    std::string order{"desc"};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_block_backtest_rows(
    const Json& rows, const BlockBacktestQuery& query,
    const BlockData& blocks = {});

class BlockBacktestService {
public:
    BlockBacktestService(std::filesystem::path root, BlockData blocks = {});
    Json query(const BlockBacktestQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_block_backtest(const std::vector<std::string>& args);

}  // namespace tdx
