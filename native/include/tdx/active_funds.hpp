#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ActiveFundQuery {
    std::string view{"securities"};
    std::string direction{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool refresh{};
    int limit{2000};
    int detail_limit{2000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_active_fund_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::pair<int, std::string>, std::string>& industries = {});
Json normalize_active_fund_detail_rows(const Json& rows);

class ActiveFundService {
public:
    using Fetcher = std::function<Json(
        const std::string& resource,
        const std::string& prefix,
        int timeout_ms)>;

    explicit ActiveFundService(BlockData data = {}, Fetcher fetcher = {});
    Json query(const ActiveFundQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    struct FetchResult {
        Json document;
        bool refreshed{};
        int age_seconds{};
        bool stale{};
        std::string upstream_error;
    };

    FetchResult fetch_master(const ActiveFundQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const ActiveFundQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::pair<int, std::string>, std::string> security_industries_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, std::pair<std::string, std::time_t>> failure_cache_;
    Fetcher fetcher_;
};

int command_market_active_funds(const std::vector<std::string>& args);

}  // namespace tdx
