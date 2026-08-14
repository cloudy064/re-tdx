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

struct FinancialScreenQuery {
    std::string dataset{"snapshot"};
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string sort{"market-cap"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_financial_screen_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_small_cap_growth_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class FinancialScreenService {
public:
    explicit FinancialScreenService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const FinancialScreenQuery& options);

private:
    Json fetch_master(const FinancialScreenQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, Json, std::less<>> caches_;
    std::map<std::string, std::time_t, std::less<>> cache_times_;
};

int command_market_financial_screen(const std::vector<std::string>& args);

}  // namespace tdx
