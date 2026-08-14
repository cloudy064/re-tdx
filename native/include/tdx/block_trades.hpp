#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct BlockTradeQuery {
    std::string view{"trades"};
    std::string query;
    std::string market;
    std::string code;
    std::string broker_id;
    std::string period{"1m"};
    std::string month;
    std::string industry;
    bool include_details{};
    bool refresh{};
    int limit{500};
    int detail_limit{1000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_block_trade_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_block_trade_history_rows(const Json& rows);
Json normalize_block_trade_intention_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security = true);
Json normalize_block_trade_month_rows(const Json& rows);
Json normalize_block_trade_industry_rows(const Json& rows);
Json normalize_block_trade_industry_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_block_trade_broker_rows(const Json& rows,
                                       const std::string& period);
Json normalize_block_trade_broker_detail_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);

class BlockTradeService {
public:
    explicit BlockTradeService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const BlockTradeQuery& options);

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

    FetchResult fetch_core(const BlockTradeQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const BlockTradeQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument core_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, CachedFailure> failure_cache_;
};

int command_market_block_trades(const std::vector<std::string>& args);

}  // namespace tdx
