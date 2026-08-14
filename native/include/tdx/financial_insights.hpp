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

struct FinancialInsightsQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string sort{"signal"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_financial_insight_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class FinancialInsightsService {
public:
    explicit FinancialInsightsService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const FinancialInsightsQuery& options);

private:
    Json fetch_master(const FinancialInsightsQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_financial_insights(const std::vector<std::string>& args);

}  // namespace tdx
