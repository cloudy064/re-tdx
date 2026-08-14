#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct ThresholdStocksQuery {
    std::string view{"history"};
    std::string universe{"high-price"};
    std::string date;
    std::string status{"all"};
    std::string market;
    std::string code;
    std::string query;
    std::string sort{"date"};
    std::string order{"desc"};
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_threshold_history_rows(const Json& rows,
                                      const std::string& universe);
Json normalize_threshold_member_rows(
    const Json& rows, const std::string& universe,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_threshold_trend_rows(const Json& rows,
                                    const std::string& universe);
void sort_threshold_rows(Json& rows, const std::string& view,
                         const std::string& sort, const std::string& order);

class ThresholdStocksService {
public:
    explicit ThresholdStocksService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ThresholdStocksQuery& options);

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

    FetchResult fetch(const std::string& resource,
                      const ThresholdStocksQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_threshold_stocks(const std::vector<std::string>& args);

}  // namespace tdx
