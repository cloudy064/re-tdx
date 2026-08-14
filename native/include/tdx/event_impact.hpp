#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct EventImpactQuery {
    std::string benchmark{"all"};
    std::string type;
    std::string query;
    std::string date_from;
    std::string date_to;
    std::string sort{"date"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_event_impact_rows(const std::string& resource, const Json& rows);

class EventImpactService {
public:
    explicit EventImpactService(std::filesystem::path jsn_root = {});
    Json query(const EventImpactQuery& options);
private:
    Json fetch_master(const EventImpactQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_event_impact(const std::vector<std::string>& args);

}  // namespace tdx
