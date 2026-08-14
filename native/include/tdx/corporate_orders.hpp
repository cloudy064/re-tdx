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

struct CorporateOrdersQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string sort{"date"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_corporate_order_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class CorporateOrdersService {
public:
    explicit CorporateOrdersService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const CorporateOrdersQuery& options);

private:
    Json fetch_master(const CorporateOrdersQuery& options, bool& refreshed,
                      int& age_seconds);
    std::map<std::pair<int, std::string>, Security> securities_;
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_corporate_orders(const std::vector<std::string>& args);

}  // namespace tdx
