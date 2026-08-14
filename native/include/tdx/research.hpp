#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct ResearchCategorySpec {
    std::string id;
    std::string label;
    std::string resource;
    std::string entity_type;
    std::vector<std::string> detail_namespaces;
};

const std::vector<ResearchCategorySpec>& research_categories();

struct ResearchQuery {
    std::string category{"institution-research"};
    std::string query;
    std::string market;
    std::string code;
    std::string detail_id;
    bool include_details{};
    bool include_text{true};
    bool refresh{};
    int limit{500};
    int detail_limit{100};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_research_master_rows(
    const Json& rows,
    const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& industry_names = {});
Json normalize_research_activity_rows(const Json& rows, bool include_text = true);
Json normalize_research_regulatory_rows(const Json& rows, bool include_text = true);
Json normalize_research_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);

class ResearchService {
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

    explicit ResearchService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::map<std::string, std::string> industry_names = {});
    Json query(const ResearchQuery& options);

private:
    FetchResult fetch_master(const ResearchQuery& options);
    FetchResult fetch_detail(const std::string& resource,
                             const ResearchQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, std::string> industry_names_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_research(const std::vector<std::string>& args);

}  // namespace tdx
