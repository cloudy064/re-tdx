#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct SessionTurnoverQuery {
    std::string universe{"a"};
    std::string sort{"after-hours"};
    std::string order{"desc"};
    std::string activity{"all"};
    std::string market;
    std::string code;
    std::string query;
    bool refresh{};
    int offset{};
    int limit{200};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

std::string session_turnover_resource_for(const std::string& universe,
                                          const std::string& sort);
Json normalize_session_turnover_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_session_turnover_rows(Json& rows, const std::string& sort,
                                const std::string& order);

class SessionTurnoverService {
public:
    explicit SessionTurnoverService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const SessionTurnoverQuery& options);

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
                      const SessionTurnoverQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_session_turnover(const std::vector<std::string>& args);

}  // namespace tdx
