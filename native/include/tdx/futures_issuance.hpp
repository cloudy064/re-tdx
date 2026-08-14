#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct FuturesIssuanceQuery {
    std::string section{"all"};
    std::string contract_key;
    std::string year;
    std::string industry_key;
    std::string placement_status{"all"};
    std::string market;
    std::string code;
    std::string search;
    bool refresh{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_futures_contract_rows(const Json& rows);
Json normalize_ipo_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_private_placement_rows(
    const Json& rows, const std::string& source_resource,
    const std::string& lifecycle,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_rights_offering_rows(
    const Json& rows, const std::string& source_resource,
    const std::string& phase,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_preferred_share_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);

class FuturesIssuanceService {
public:
    struct CachedResource {
        Json document;
        std::time_t fetched_at{};
    };

    explicit FuturesIssuanceService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const FuturesIssuanceQuery& options);

private:
    Json fetch_resources(const std::vector<std::string>& resources,
                         const FuturesIssuanceQuery& options,
                         Json& cache_status);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedResource, std::less<>> cache_;
};

int command_market_futures_issuance(const std::vector<std::string>& args);

}  // namespace tdx
