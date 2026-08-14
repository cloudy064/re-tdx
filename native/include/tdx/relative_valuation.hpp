#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct RelativeValuationQuery {
    std::string code;
    std::string index_type{"broad"};
    std::string benchmark{"000001"};
    std::string method{"pe-ttm"};
    std::string start_date;
    std::string end_date;
    bool refresh{};
    int limit{4000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_relative_valuation_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_relative_valuation_history_rows(const Json& rows);

class RelativeValuationService {
public:
    RelativeValuationService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const RelativeValuationQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult {
        Json document;
        bool hit{};
        bool stale{};
        int age_seconds{};
        std::string upstream_error;
    };

    FetchResult fetch_cached(
        std::map<std::string, CachedDocument>& cache,
        const std::string& key, const RelativeValuationQuery& options,
        const std::function<Json()>& loader);
    FetchResult fetch_master(
        const RelativeValuationQuery& options,
        const std::map<std::string, std::string>& replacements,
        const std::string& key);
    FetchResult fetch_detail(
        const RelativeValuationQuery& options,
        const std::map<std::string, std::string>& replacements,
        const std::string& key);

    std::filesystem::path root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_relative_valuation(const std::vector<std::string>& args);

}  // namespace tdx
