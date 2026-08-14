#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct BlockRotationQuery {
    std::string category{"all"};
    std::string period{"1w"};
    std::string sort{"last-date"};
    std::string order{"desc"};
    std::string signal{"all"};
    std::string code;
    std::string query;
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

std::string block_rotation_resource_for(const std::string& category);
Json normalize_block_rotation_rows(const Json& rows, const std::string& category,
                                   const BlockData& blocks = {});
void sort_block_rotation_rows(Json& rows, const std::string& sort,
                              const std::string& period,
                              const std::string& order);

class BlockRotationService {
public:
    explicit BlockRotationService(BlockData blocks = {});
    Json query(const BlockRotationQuery& options);

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

    FetchResult fetch(const std::string& category,
                      const BlockRotationQuery& options);

    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_block_rotation(const std::vector<std::string>& args);

}  // namespace tdx
