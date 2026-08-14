#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct MarginQuery {
    std::string view{"market"};
    std::string category{"balance"};
    std::string query;
    std::string market;
    std::string code;
    std::string date;
    std::string group_id;
    bool refresh{};
    int limit{2000};
    int cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

struct StockConnectQuery {
    std::string view{"flows"};
    std::string category{"northbound-total"};
    std::string query;
    std::string market;
    std::string code;
    std::string date;
    std::string channel;
    std::string group_id;
    bool refresh{};
    int limit{3000};
    int cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_margin_market_rows(const Json& rows);
Json normalize_margin_transfer_rows(const Json& rows);
Json normalize_margin_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_margin_classification_rows(const Json& rows,
                                          const std::string& classification);
Json normalize_margin_classification_history_rows(const Json& rows);
Json normalize_stock_connect_flow_rows(const Json& rows);
Json normalize_stock_connect_holding_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_stock_connect_history_rows(const Json& rows);
Json normalize_stock_connect_chart_rows(const Json& rows);
Json normalize_stock_connect_southbound_member_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_stock_connect_southbound_trend_rows(const Json& rows);
Json normalize_stock_connect_activity_rows(
    const Json& rows, const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_stock_connect_industry_rows(const Json& rows,
                                           const std::string& category);
Json normalize_stock_connect_active_rows(
    const Json& rows, const std::string& channel,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class LeverageService {
public:
    explicit LeverageService(BlockData data = {});
    Json query_margin(const MarginQuery& options);
    Json query_stock_connect(const StockConnectQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };
    FetchResult fetch_resource(const std::string& resource, bool refresh,
                               int ttl_seconds, int timeout_ms);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, std::pair<std::string, std::time_t>> failure_cache_;
};

int command_market_margin(const std::vector<std::string>& args);
int command_market_stock_connect(const std::vector<std::string>& args);

}  // namespace tdx
