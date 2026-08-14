#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct InstitutionLhbPeriod {
    std::string name;
    std::string label;
    std::string resource;
    std::string unit_id;
};

struct InstitutionLhbQuery {
    std::string period{"week"};
    std::string direction{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool include_details{};
    bool refresh{};
    int limit{3000};
    int detail_limit{1000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

const std::vector<InstitutionLhbPeriod>& institution_lhb_periods();
Json normalize_institution_lhb_rows(
    const Json& rows, const InstitutionLhbPeriod& period,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_institution_lhb_event_rows(const Json& rows);

class InstitutionLhbService {
public:
    explicit InstitutionLhbService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const InstitutionLhbQuery& options);

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

    FetchResult fetch_master(const InstitutionLhbQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const InstitutionLhbQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, std::pair<std::string, std::time_t>> failure_cache_;
};

int command_market_institution_lhb(const std::vector<std::string>& args);

}  // namespace tdx
