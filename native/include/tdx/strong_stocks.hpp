#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct StrongStocksQuery {
    std::string view{"intervals"};
    std::string market;
    std::string code;
    std::string interval_id;
    std::string query;
    std::string from;
    std::string to;
    std::string sort;
    std::string order;
    int min_trading_days{};
    int min_limit_up_days{};
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_strong_stock_intervals(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_strong_stock_detail(
    const Json& rows, const Json& interval,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_strong_stock_rows(Json& rows, const std::string& view,
                            const std::string& sort,
                            const std::string& order);

class StrongStocksService {
public:
    explicit StrongStocksService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const StrongStocksQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };

    Json fetch(const std::string& resource, bool refresh,
               int cache_ttl_seconds, int timeout_ms, bool& fetched);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_strong_stocks(const std::vector<std::string>& args);

}  // namespace tdx
