#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct LimitReviewQuery {
    std::string view{"catalog"};
    std::string category{"all"};
    std::string market;
    std::string code;
    std::string date;
    std::string query;
    bool refresh{};
    int offset{};
    int limit{200};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_limit_review_current_rows(
    const Json& rows, const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_limit_review_annual_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_limit_review_market_history_rows(const Json& rows);
Json normalize_limit_review_daily_rows(
    const Json& rows, const std::string& category, const std::string& date,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_limit_review_security_history_rows(const Json& rows);

class LimitReviewService {
public:
    explicit LimitReviewService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const LimitReviewQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };

    Json fetch_resource(const std::string& resource,
                        const LimitReviewQuery& options,
                        bool allow_missing = false);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_limit_review(const std::vector<std::string>& args);

}  // namespace tdx
