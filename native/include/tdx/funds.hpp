#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct IntradayFundsQuery {
    std::string market;
    std::string code;
    std::string industry;
    bool all_industries{};
    bool refresh{};
    int cache_ttl_seconds{15};
    int timeout_ms{15000};
};

struct ResearchIndustry {
    std::string market{"1"};
    std::string code;
    std::string name;
};

Json normalize_intraday_fund_record(
    const Json& record,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool detail,
    const std::map<std::string, std::string>& industry_names = {});

class IntradayFundsService {
public:
    using Fetcher = std::function<Json(
        const std::string& request_id,
        const std::string& market,
        const std::string& code,
        int timeout_ms)>;

    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    struct FetchResult {
        Json document;
        bool refreshed{};
        int age_seconds{};
        bool stale{};
        std::string upstream_error;
    };

    IntradayFundsService(std::filesystem::path root, const BlockData& blocks,
                         Fetcher fetcher = {});

    Json query(const IntradayFundsQuery& options);
    const ResearchIndustry* industry_for_security(int market_id,
                                                  const std::string& code) const;

private:

    FetchResult fetch_master(const IntradayFundsQuery& options);
    FetchResult fetch_detail(const std::string& market, const std::string& code,
                             const IntradayFundsQuery& options);
    Json build_master(const FetchResult& master, const IntradayFundsQuery& options);
    Json build_industry(const std::string& code, const FetchResult& master,
                        const FetchResult& detail, const IntradayFundsQuery& options);
    Json build_security(int market_id, const std::string& code,
                        const ResearchIndustry& industry,
                        const FetchResult& master, const FetchResult& detail,
                        const IntradayFundsQuery& options);
    Json build_all(const FetchResult& master, const IntradayFundsQuery& options);

    std::filesystem::path root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::pair<int, std::string>, ResearchIndustry> security_industries_;
    std::map<std::string, ResearchIndustry> industries_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
    Fetcher fetcher_;
};

Json intraday_funds_cache_document(
    const IntradayFundsService::FetchResult& master,
    const IntradayFundsService::FetchResult* detail,
    int ttl_seconds);

int command_market_funds(const std::vector<std::string>& args);

}  // namespace tdx
