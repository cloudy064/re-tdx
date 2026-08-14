#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ValuationQuery {
    std::string market;
    std::string code;
    std::string detail_id;
    std::string start_date;
    std::string end_date;
    bool include_details{};
    bool refresh{};
    int limit{4000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_valuation_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_valuation_fund_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json merge_valuation_history_rows(const Json& pe_rows, const Json& pb_rows);

class ValuationService {
public:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    struct FetchResult {
        Json document;
        bool refreshed{};
        int age_seconds{};
    };

    explicit ValuationService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ValuationQuery& options);

private:
    FetchResult fetch_master(const ValuationQuery& options);
    FetchResult fetch_detail(const std::string& detail_id,
                             const ValuationQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_valuation(const std::vector<std::string>& args);

}  // namespace tdx
