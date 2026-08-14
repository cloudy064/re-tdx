#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct EconomicIndicatorQuery {
    std::string view{"catalog"};
    std::string indicator_id;
    std::string query;
    std::string sort{"update-date"};
    std::string order{"desc"};
    bool include_history{true};
    bool include_related{true};
    bool include_quotes{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int history_limit{2000};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{300};
    int quote_cache_ttl_seconds{5};
    int timeout_ms{15000};
};

Json normalize_economic_indicator_rows(const Json& rows);
Json normalize_economic_indicator_history(const Json& rows);
Json normalize_economic_indicator_relations(
    const Json& rows, const Json& quote_rows = Json::array(),
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_economic_indicator_rows(Json& rows, const std::string& sort,
                                  const std::string& order);

class EconomicIndicatorService {
public:
    EconomicIndicatorService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const EconomicIndicatorQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };

    // Everything the indicator view accumulates while walking its optional
    // detail resources. Grouping it keeps the two detail steps to one
    // out-parameter instead of eight.
    struct QueryState {
        Json sources = Json::array();
        Json errors = Json::array();
        Json history = Json::array();
        Json related = Json::array();
        Json selected = Json(nullptr);
        Json quote_source = Json(nullptr);
        Json summary = Json::object();
        std::uint64_t matched{};
        bool detail_refreshed{};
        bool quote_refreshed{};
        int detail_age{};
        int quote_age{};
    };

    FetchResult fetch_resource(const std::string& resource, bool refresh,
                               int ttl_seconds, int timeout_ms);
    FetchResult fetch_quotes(const std::vector<std::string>& securities,
                             const EconomicIndicatorQuery& options);

    // Optional detail resources for the indicator view. Each records its own
    // failure into state.errors rather than propagating, so a missing series
    // degrades the response to "partial" instead of failing the query.
    void load_history(const EconomicIndicatorQuery& options, QueryState& state);
    void load_related(const EconomicIndicatorQuery& options, QueryState& state);
    void attach_quotes(const EconomicIndicatorQuery& options, const Json& paged_raw,
                       QueryState& state);

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, CachedDocument> quote_cache_;
};

int command_market_economic_indicators(const std::vector<std::string>& args);

}  // namespace tdx
