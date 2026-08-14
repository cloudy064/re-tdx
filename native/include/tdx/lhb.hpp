#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct LhbView {
    std::string name;
    std::string resource;
};

struct LhbQuery {
    std::string market;
    std::string code;
    std::string event_id;
    bool include_details{};
    bool refresh{};
    int limit{10};
    int master_cache_ttl_seconds{60};
    int detail_cache_ttl_seconds{300};
    int timeout_ms{10000};
};

const std::vector<LhbView>& lhb_views();
Json aggregate_lhb_master_documents(
    const Json& documents,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::string& generated_at = {});
Json normalize_lhb_detail_rows(const Json& rows);

class LhbService {
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

    LhbService(std::map<std::pair<int, std::string>, Security> securities);
    Json query(const LhbQuery& options);

private:
    FetchResult fetch_master(const LhbQuery& options);
    std::map<std::string, FetchResult> fetch_details(
        const std::vector<std::string>& event_ids, const LhbQuery& options,
        Json& errors);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_lhb(const std::vector<std::string>& args);

}  // namespace tdx
