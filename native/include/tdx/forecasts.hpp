#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ForecastQuery {
    std::string view{"industries"};
    std::string category{"all"};
    std::string query;
    std::string industry;
    std::string market;
    std::string code;
    std::string report_period;
    bool include_details{};
    bool refresh{};
    int limit{1000};
    int detail_limit{5000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_forecast_industry_rows(
    const Json& rows, const std::map<std::string, std::string>& industry_names);
Json normalize_forecast_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_hong_kong_forecast_rows(const Json& rows);

class ForecastService {
public:
    explicit ForecastService(BlockData data = {});
    Json query(const ForecastQuery& options);

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

    FetchResult fetch_core(const ForecastQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const ForecastQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, std::string> industry_names_;
    std::map<std::pair<int, std::string>, std::string> security_industries_;
    CachedDocument core_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, std::pair<std::string, std::time_t>> failure_cache_;
};

int command_market_forecasts(const std::vector<std::string>& args);

}  // namespace tdx
