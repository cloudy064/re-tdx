#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <string>
#include <vector>

namespace tdx {

struct LimitLadderQuery {
    std::string category{"all"};
    std::string sort{"total-height"};
    std::string order{"desc"};
    std::string activity{"all"};
    std::string code;
    std::string query;
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_limit_ladder_rows(const Json& rows,
                                 const BlockData& blocks = {});
void sort_limit_ladder_rows(Json& rows, const std::string& sort,
                            const std::string& order);

class LimitLadderService {
public:
    explicit LimitLadderService(BlockData blocks = {});
    Json query(const LimitLadderQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    struct FetchResult {
        Json document;
        bool refreshed{};
        int age_seconds{};
    };

    FetchResult fetch(const LimitLadderQuery& options);

    BlockData blocks_;
    CachedDocument cache_;
};

int command_market_limit_ladder(const std::vector<std::string>& args);

}  // namespace tdx
