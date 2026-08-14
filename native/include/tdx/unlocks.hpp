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

struct UnlockQuery {
    std::string view{"calendar"};
    std::string query;
    std::string market;
    std::string code;
    std::string detail_id;
    std::string start_date;
    std::string end_date;
    std::string progress;
    std::string reason;
    bool include_details{};
    bool refresh{};
    int limit{500};
    int detail_limit{1000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_unlock_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_unlock_shareholder_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_recent_large_unlock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_monthly_unlock_pressure_rows(const Json& rows);

class UnlockService {
public:
    explicit UnlockService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const UnlockQuery& options);

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

    FetchResult fetch_master(const UnlockQuery& options);
    FetchResult fetch_recent_large(const UnlockQuery& options);
    FetchResult fetch_monthly_pressure(const UnlockQuery& options);
    FetchResult fetch_detail(const std::string& resource,
                             const UnlockQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::filesystem::path jsn_root_;
    CachedDocument master_cache_;
    CachedDocument recent_large_cache_;
    CachedDocument monthly_pressure_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
    std::map<std::string, CachedFailure> detail_failure_cache_;
};

int command_market_unlocks(const std::vector<std::string>& args);

}  // namespace tdx
