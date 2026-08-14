#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct CapitalStrengthQuery {
    std::string view{"ranking"};
    std::string period{"5d"};
    std::string market;
    std::string code;
    std::string query;
    std::string sort;
    std::string order;
    int min_periods{2};
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_capital_strength_rows(
    const Json& rows, const std::string& period,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json compose_capital_strength_confluence(const Json& period_rows);
void sort_capital_strength_rows(Json& rows, const std::string& view,
                                const std::string& sort,
                                const std::string& order);

class CapitalStrengthService {
public:
    explicit CapitalStrengthService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const CapitalStrengthQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_capital_strength(const std::vector<std::string>& args);

}  // namespace tdx
