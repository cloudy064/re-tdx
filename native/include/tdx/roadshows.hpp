#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct RoadshowQuery {
    std::string market;
    std::string code;
    std::string query;
    std::string type;
    std::string status;
    std::string start_date;
    std::string end_date;
    int offset{};
    int limit{100};
    bool refresh{};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

using RoadshowFetcher = std::function<Json(const std::string& key, int timeout_ms)>;

Json normalize_roadshow_rows(
    const Json& rows, bool security_detail, int selected_market = -1,
    const std::string& selected_code = {}, const std::string& selected_name = {});

class RoadshowService {
public:
    RoadshowService(std::filesystem::path root, BlockData blocks,
                    RoadshowFetcher fetcher = {});
    Json query(const RoadshowQuery& options);

private:
    struct CacheEntry {
        Json rows;
        std::time_t fetched_at{};
    };

    std::filesystem::path root_;
    BlockData blocks_;
    RoadshowFetcher fetcher_;
    std::map<std::string, CacheEntry> cache_;
    std::mutex cache_mutex_;
};

int command_market_roadshows(const std::vector<std::string>& args);

}  // namespace tdx
