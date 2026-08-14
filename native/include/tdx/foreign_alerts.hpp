#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ForeignAlertQuery {
    std::string status{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool include_details{};
    bool refresh{};
    int limit{1000};
    int detail_limit{2000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

std::string foreign_alert_status_key(const std::string& status);
Json normalize_foreign_alert_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_foreign_alert_history_rows(const Json& rows);

class ForeignAlertService {
public:
    explicit ForeignAlertService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ForeignAlertQuery& options);

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

    FetchResult fetch_master(const ForeignAlertQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const ForeignAlertQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, std::pair<std::string, std::time_t>> failure_cache_;
};

int command_market_foreign_alerts(const std::vector<std::string>& args);

}  // namespace tdx
