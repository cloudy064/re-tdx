#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct OwnershipQuery {
    std::string view{"changes"};
    std::string category{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string institution_id;
    bool include_details{};
    bool refresh{};
    int limit{1000};
    int detail_limit{2000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_ownership_change_rows(
    const Json& rows, const std::string& direction,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security = true);
Json normalize_ownership_plan_rows(
    const Json& rows, const std::string& direction,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_ownership_ranking_rows(
    const Json& rows, const std::string& direction, const std::string& metric,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_shareholder_count_rows(
    const Json& rows, const std::string& board,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_insider_change_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security = true);
Json normalize_no_reduction_commitment_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_pledge_latest_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_pledge_risk_rows(
    const Json& rows, const std::string& kind,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_pledge_release_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_pledge_history_rows(const Json& rows);
Json normalize_ownership_statistics_rows(const Json& rows,
                                         const std::string& period);
Json normalize_ownership_change_count_trend_rows(const Json& rows);
Json normalize_pledge_month_rows(const Json& rows);
Json normalize_pledge_institution_rows(const Json& rows,
                                       const std::string& category);
Json normalize_pledge_institution_detail_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);

class OwnershipService {
public:
    explicit OwnershipService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const OwnershipQuery& options);

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

    FetchResult fetch_core(const OwnershipQuery& options);
    FetchResult fetch_rankings(const OwnershipQuery& options);
    FetchResult fetch_shareholder_counts(const OwnershipQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const OwnershipQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument core_cache_;
    CachedDocument rankings_cache_;
    CachedDocument shareholder_counts_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, CachedFailure> failure_cache_;
};

int command_market_ownership(const std::vector<std::string>& args);

}  // namespace tdx
