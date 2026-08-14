#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct EtfFlowQuery {
    std::string view{"stocks"};
    std::string direction{"all"};
    std::string sort{"total-flow"};
    std::string query;
    std::string market;
    std::string code;
    std::string industry;
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_etf_flow_rows(
    const Json& rows, bool industry_rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& industry_names);

class EtfFlowService {
public:
    explicit EtfFlowService(BlockData data = {});
    Json query(const EtfFlowQuery& options);

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

    FetchResult fetch(bool industries, const EtfFlowQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, std::string> industry_names_;
    std::map<std::pair<int, std::string>, std::string> security_industries_;
    CachedDocument stock_cache_;
    CachedDocument industry_cache_;
};

int command_market_etf_flows(const std::vector<std::string>& args);

}  // namespace tdx
