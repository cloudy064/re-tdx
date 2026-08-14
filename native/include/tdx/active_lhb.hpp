#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct ActiveLhbPeriod {
    std::string name;
    std::string label;
    std::string resource;
    std::string unit_id;
};

struct ActiveLhbQuery {
    std::string period{"5d"};
    std::string market;
    std::string code;
    std::string query;
    std::string direction{"all"};
    std::string sort{"events"};
    std::string order{"desc"};
    bool include_details{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int detail_limit{500};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{300};
    int timeout_ms{15000};
};

const std::vector<ActiveLhbPeriod>& active_lhb_periods();
Json normalize_active_lhb_rows(
    const Json& rows, const ActiveLhbPeriod& period,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_active_lhb_event_rows(const Json& rows);
void sort_active_lhb_rows(Json& rows, const std::string& sort,
                          const std::string& order);

class ActiveLhbService {
public:
    explicit ActiveLhbService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ActiveLhbQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };
    FetchResult fetch_master(const ActiveLhbQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const ActiveLhbQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
};

int command_market_active_lhb(const std::vector<std::string>& args);

}  // namespace tdx
