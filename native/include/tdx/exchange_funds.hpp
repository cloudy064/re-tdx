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

struct ExchangeFundQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool include_quotes{};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int quote_cache_ttl_seconds{15};
    int timeout_ms{15000};
};

Json normalize_exchange_fund_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

void apply_exchange_fund_quotes(Json& rows, const Json& quotes);

class ExchangeFundService {
public:
    explicit ExchangeFundService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const ExchangeFundQuery& options);

private:
    Json fetch_master(const ExchangeFundQuery& options, bool& refreshed,
                      int& age_seconds);
    Json fetch_quotes(const std::vector<std::string>& requested,
                      const ExchangeFundQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path root_;
    BlockData blocks_;
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
    Json quote_cache_;
    std::string quote_cache_key_;
    std::time_t quote_cache_time_{};
};

int command_market_exchange_funds(const std::vector<std::string>& args);

}  // namespace tdx
