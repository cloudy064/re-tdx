#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct FlowFollowupQuery {
    std::string view{"margin"};
    std::string start_date;
    std::string end_date;
    bool available_only{};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{300};
    int timeout_ms{30000};
};

Json normalize_flow_followup_rows(const std::string& view, const Json& rows);
Json normalize_flow_model_rows(const std::string& view, const Json& rows);
Json normalize_flow_model_summary(const std::string& view, const Json& rows);

class FlowFollowupService {
public:
    explicit FlowFollowupService(std::filesystem::path root);
    Json query(const FlowFollowupQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    std::filesystem::path root_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_flow_followup(const std::vector<std::string>& args);

}  // namespace tdx
