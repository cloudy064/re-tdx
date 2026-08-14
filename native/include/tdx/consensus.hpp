#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ConsensusCategorySpec {
    std::string id;
    std::string label;
    std::string resource;
};

const std::vector<ConsensusCategorySpec>& consensus_categories();

struct ConsensusQuery {
    std::string category{"latest"};
    std::string query;
    std::string market;
    std::string code;
    bool include_details{};
    bool include_report_text{true};
    bool refresh{};
    int limit{500};
    int report_limit{100};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_consensus_master_rows(
    const Json& rows,
    const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_consensus_report_rows(const Json& rows, bool include_report_text = true);

class ConsensusService {
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

    explicit ConsensusService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ConsensusQuery& options);

private:
    FetchResult fetch_master(const ConsensusQuery& options);
    FetchResult fetch_detail(int market_id, const std::string& code,
                             const ConsensusQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_consensus(const std::vector<std::string>& args);

}  // namespace tdx
