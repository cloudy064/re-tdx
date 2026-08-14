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

struct RepurchaseQuery {
    std::string view{"plans"};
    std::string query;
    std::string market;
    std::string code;
    std::string segment{"a"};
    std::string year;
    bool include_details{};
    bool refresh{};
    int limit{1000};
    int detail_limit{2000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_repurchase_plan_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_repurchase_month_rows(const Json& rows);
Json normalize_repurchase_annual_rows(const Json& rows,
                                      const std::string& segment);
Json normalize_hk_repurchase_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security = true);

class RepurchaseService {
public:
    explicit RepurchaseService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const RepurchaseQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    struct CachedFailure {
        std::string message;
        std::time_t fetched_at{};
    };
    struct FetchResult {
        Json document;
        bool refreshed{};
        int age_seconds{};
    };

    FetchResult fetch_core(const RepurchaseQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const RepurchaseQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::filesystem::path jsn_root_;
    CachedDocument core_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, CachedFailure> failure_cache_;
};

int command_market_repurchases(const std::vector<std::string>& args);

}  // namespace tdx
