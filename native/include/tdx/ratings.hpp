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

struct RatingQuery {
    std::string view{"hong-kong"};
    std::string stance{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string industry;
    bool include_details{};
    bool refresh{};
    int limit{2000};
    int detail_limit{1000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

std::string classify_rating_stance(const std::string& rating);
std::map<std::string, std::string> load_hong_kong_security_names(
    const std::filesystem::path& root);
Json normalize_hong_kong_rating_rows(
    const Json& rows, const std::map<std::string, std::string>& names = {});
Json normalize_united_states_rating_rows(const Json& rows);
Json normalize_industry_rating_rows(
    const Json& rows, const std::map<std::string, std::string>& names = {});
Json normalize_rating_report_rows(const Json& rows, const std::string& view);

class RatingService {
public:
    RatingService(std::filesystem::path root = {}, BlockData data = {});
    Json query(const RatingQuery& options);

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

    FetchResult fetch_master(const RatingQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const RatingQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, std::string> hong_kong_names_;
    std::map<std::string, std::string> industry_names_;
    std::map<std::pair<int, std::string>, std::string> security_industries_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, std::pair<std::string, std::time_t>> failure_cache_;
};

int command_market_ratings(const std::vector<std::string>& args);

}  // namespace tdx
