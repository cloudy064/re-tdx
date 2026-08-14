#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct RecentWatchQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool include_raw{true};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_recent_watch_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class RecentWatchService {
public:
    explicit RecentWatchService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const RecentWatchQuery& options);
private:
    Json fetch_master(const RecentWatchQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_recent_watch(const std::vector<std::string>& args);

}  // namespace tdx
