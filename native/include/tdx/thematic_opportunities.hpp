#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ThematicOpportunityQuery {
    std::string view{"catalog"};
    std::string type{"all"};
    std::string group_id;
    std::string market;
    std::string code;
    std::string query;
    std::string sort{"name"};
    std::string order{"asc"};
    bool include_detail{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int master_cache_ttl_seconds{900};
    int detail_cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_opportunity_groups(
    const Json& rows, const std::string& type,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_opportunity_group_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_legacy_client_theme_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_completed_hype_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_active_hype_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class ThematicOpportunityService {
public:
    explicit ThematicOpportunityService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ThematicOpportunityQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };

    FetchResult fetch_master(const ThematicOpportunityQuery& options);
    FetchResult fetch_detail(const std::string& resource,
                             const ThematicOpportunityQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_thematic_opportunities(const std::vector<std::string>& args);

}  // namespace tdx
