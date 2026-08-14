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

struct CalendarQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string event_id;
    std::string date_from;
    std::string date_to;
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

class CalendarService {
public:
    explicit CalendarService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const CalendarQuery& options);

private:
    Json fetch_master(const CalendarQuery& options, bool& refreshed,
                      int& age_seconds);
    std::map<std::pair<int, std::string>, Security> securities_;
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

Json normalize_calendar_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

int command_market_calendar(const std::vector<std::string>& args);

}  // namespace tdx
