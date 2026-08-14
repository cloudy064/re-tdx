#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct ReverseRepoQuery {
    std::string view{"rates"};
    std::string market{"all"};
    std::string code;
    std::string query;
    std::string sort{"rate"};
    std::string order{"desc"};
    int min_term_days{};
    int max_term_days{};
    int principal_yuan{100000};
    bool include_quotes{true};
    bool refresh{};
    int offset{};
    int limit{100};
    int cache_ttl_seconds{300};
    int quote_cache_ttl_seconds{5};
    int timeout_ms{15000};
};

Json normalize_reverse_repo_rows(
    const Json& schedule_rows, const Json& quote_rows, int principal_yuan,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_reverse_repo_rows(Json& rows, const std::string& sort,
                            const std::string& order);

class ReverseRepoService {
public:
    ReverseRepoService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ReverseRepoQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };

    Json fetch_schedule(bool refresh, int cache_ttl_seconds,
                        int timeout_ms, bool& fetched);
    Json fetch_quotes(const std::vector<std::string>& securities,
                      bool refresh, int cache_ttl_seconds,
                      int timeout_ms, bool& fetched);

    std::filesystem::path root_;
    BlockData names_;
    CachedDocument schedule_cache_;
    CachedDocument quote_cache_;
};

int command_market_reverse_repo(const std::vector<std::string>& args);

}  // namespace tdx
