#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct TotalReturnGapQuery {
    std::string query;
    std::string market;
    std::string code;
    int year{};
    int month{};
    bool refresh{};
    int limit{1000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_total_return_gap_rows(const Json& rows);

class TotalReturnGapService {
public:
    explicit TotalReturnGapService(std::filesystem::path root);
    Json query(const TotalReturnGapQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    std::filesystem::path root_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_total_return_gap(const std::vector<std::string>& args);

}  // namespace tdx
