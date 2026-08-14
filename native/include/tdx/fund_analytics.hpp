#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct FundAnalyticsQuery {
    std::string view{"risk"};
    std::string query;
    std::string fund_code;
    std::string style{"005001"};
    int fund_size{};
    int fund_age{};
    int benchmark{};
    std::string start_date;
    std::string end_date;
    std::string report_date;
    std::string estimate_date;
    double risk_free_rate{3.0};
    bool all_pages{true};
    bool refresh{};
    int limit{10000};
    int max_pages{100};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_fund_analytics_rows(const Json& rows, const std::string& view);

class FundAnalyticsService {
public:
    explicit FundAnalyticsService(std::filesystem::path root);
    Json query(const FundAnalyticsQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    std::filesystem::path root_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_fund_analytics(const std::vector<std::string>& args);

}  // namespace tdx
