#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct FundStatisticsQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_fund_statistics_rows(const std::string& resource,
                                    const Json& rows);

class FundStatisticsService {
public:
    explicit FundStatisticsService(std::filesystem::path jsn_root = {});
    Json query(const FundStatisticsQuery& options);

private:
    Json fetch_master(const FundStatisticsQuery& options, bool& refreshed,
                      int& age_seconds);

    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_fund_statistics(const std::vector<std::string>& args);

}  // namespace tdx
