#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct IndustryProfileQuery {
    std::string industry;
    std::string market;
    std::string code;
    bool include_catalog{true};
    bool refresh{};
    int detail_limit{1000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_industry_holdings_rows(
    const Json& rows, const std::string& period_key,
    const std::string& period_label,
    const std::map<std::string, Block>& blocks);
Json normalize_industry_holdings_history_rows(const Json& rows);
Json normalize_industry_shareholder_rows(
    const Json& rows, const std::map<std::string, Block>& blocks);
Json normalize_industry_shareholder_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);

class IndustryProfileService {
public:
    explicit IndustryProfileService(const BlockData& blocks);
    Json query(const IndustryProfileQuery& options);

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

    FetchResult fetch_master(const IndustryProfileQuery& options);
    FetchResult fetch_detail(const std::string& resource,
                             const IndustryProfileQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, Block> blocks_;
    std::map<std::string, std::string> parents_;
    std::map<std::string, std::vector<std::string>> children_;
    std::map<std::string, std::set<std::pair<int, std::string>>> block_members_;
    std::map<std::pair<int, std::string>, std::vector<std::string>> security_blocks_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_industry_profile(const std::vector<std::string>& args);

}  // namespace tdx
