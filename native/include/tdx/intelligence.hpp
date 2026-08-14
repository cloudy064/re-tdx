#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct IntelligenceQuery {
    std::string view{"attention"};
    std::string category{"all"};
    std::string highlight_type;
    std::string sort;
    std::string order{"desc"};
    std::string query;
    std::string market;
    std::string code;
    std::string topic_id;
    std::string event_id;
    std::string category_id;
    bool refresh{};
    int offset{};
    int limit{500};
    int member_limit{3000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_attention_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_risk_rows(
    const Json& rows,
    const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_highlight_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_value_attention_categories(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_value_attention_detail_rows(
    const Json& rows, const std::string& category_id,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_highlight_rows(Json& rows, const std::string& sort,
                         const std::string& order);
Json normalize_intelligence_event_rows(
    const Json& rows,
    const std::string& source,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json build_intelligence_graph(const Json& events, int member_limit);
Json normalize_intelligence_topic_rows(const Json& rows);
Json normalize_intelligence_timeline_rows(const Json& rows,
                                          const std::string& kind);
Json normalize_market_anomaly_rows(const Json& rows);
Json normalize_intelligence_event_member_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class IntelligenceService {
public:
    explicit IntelligenceService(BlockData data = {});
    Json query(const IntelligenceQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchSet { bool refreshed{}; int age_seconds{}; Json sources{Json::array()}; };

    FetchSet ensure_resources(const std::vector<std::string>& resources,
                              bool refresh, int ttl_seconds, int timeout_ms);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> resource_cache_;
};

int command_market_intelligence(const std::vector<std::string>& args);

}  // namespace tdx
