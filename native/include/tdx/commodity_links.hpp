#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct CommodityLinksQuery {
    std::string view{"commodities"};
    std::string commodity_id;
    std::string theme_id;
    std::string driver_id;
    std::string market;
    std::string code;
    std::string query;
    std::string sort;
    std::string order;
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_commodity_rows(const Json& rows);
Json normalize_price_theme_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_theme_stock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_theme_driver_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_driver_stock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_commodity_stock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_commodity_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class CommodityLinksService {
public:
    explicit CommodityLinksService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const CommodityLinksQuery& options);

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

int command_market_commodity_links(const std::vector<std::string>& args);

}  // namespace tdx
